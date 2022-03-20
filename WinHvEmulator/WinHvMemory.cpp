#include "WinHvMemory.hpp"
#include "WinHvMemoryInternal.hpp"
#include "WinHvHelpers.hpp"
#include "WinHvProcessor.hpp"
#include "WinHvUtils.hpp"
#include "winbase.h"
#include "WinHvAllocationTracker.hpp"

/**
 * @brief A routine to translate a <WHSE_MEMORY_ACCESS_FLAGS>
 *
 * Internal routine
 * A routine to translate a <WHSE_MEMORY_ACCESS_FLAGS> to a
 * Protection Flags value compatible with the <VirtualAlloc> API
 *
 * @param Flags The flags to translate
 * @return The translated flags
 */
constexpr uint32_t AccessFlagsToProtectionFlags( WHSE_MEMORY_ACCESS_FLAGS Flags ) {
	uint32_t protectionFlags = 0;

	if ( Flags == WHvMapGpaRangeFlagNone )
		return PAGE_NOACCESS;

	if ( HAS_FLAGS( Flags, WHvMapGpaRangeFlagRead ) )
		protectionFlags |= ( 1 << 1 );

	if ( HAS_FLAGS( Flags, WHvMapGpaRangeFlagWrite ) )
		protectionFlags <<= 1;

	if ( HAS_FLAGS( Flags, WHvMapGpaRangeFlagExecute ) )
		protectionFlags <<= 4;

	return protectionFlags;
}

/**
 * @brief Allocate memory in guest physical address space (backed by host memory)
 *
 * Allocate memory in guest physical address space (backed by host memory), mapping twice
 * on the same guest physical memory address will replace any existing mapping but will not free
 * the existing host backing memory.
 *
 * @param Partition The VM partition
 * @param HostVa The host virtual memory address backing the guest physical memory
 * @param GuestPa The guest physical memory address
 * @param Size The size of the allocated memory
 * @param Flags The flags that describe the allocated memory (Read Write Execute)
 * @return A result code
 */
HRESULT WhSeAllocateGuestPhysicalMemory( WHSE_PARTITION* Partition, PVOID* HostVa, uintptr_t GuestPa, size_t* Size, WHSE_MEMORY_ACCESS_FLAGS Flags ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( HostVa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( *HostVa != nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto size = ALIGN_UP( *Size );

	// Allocate memory into host
	//
	auto protectionFlags = AccessFlagsToProtectionFlags( Flags );
	auto allocatedHostVa = ::VirtualAlloc( nullptr, size, MEM_COMMIT | MEM_RESERVE, protectionFlags );
	if ( allocatedHostVa == nullptr )
		return WhSeGetLastHresult();

	// Create the allocated range into the guest physical address space
	//
	auto hresult = ::WHvMapGpaRange( Partition->Handle, allocatedHostVa, static_cast< WHV_GUEST_PHYSICAL_ADDRESS >( GuestPa ), size, Flags );
	if ( FAILED( hresult ) ) {
		if ( allocatedHostVa != nullptr )
			::VirtualFree( allocatedHostVa, 0, MEM_RELEASE );

		return hresult;
	}

	WHSE_ALLOCATION_NODE node {
		.GuestPhysicalAddress = GuestPa,
		.HostVirtualAddress = allocatedHostVa,
		.Size = size
	};

	hresult = WhSeInsertAllocationTrackingNode( Partition, node );
	if ( FAILED( hresult ) )
		return hresult;

	*HostVa = allocatedHostVa;
	*Size = size;

	return hresult;
}

/**
 * @brief Map memory from host to guest physical address space (backed by host memory)
 *
 * Map host memory to guest physical memory, mapping twice
 * on the same guest physical memory address will replace any existing mapping but will not free
 * the existing host backing memory.
 * 
 * @param Partition The VM partition
 * @param HostVa The host virtual memory address backing the guest physical memory
 * @param GuestPa The guest physical memory address
 * @param Size The size of the allocated memory
 * @param Flags The flags that describe the allocated memory (Read Write Execute)
 * @return A result code
 */
HRESULT WHSEAPI WhSeMapHostToGuestPhysicalMemory( WHSE_PARTITION* Partition, PVOID HostVa, uintptr_t GuestPa, size_t Size, WHSE_MEMORY_ACCESS_FLAGS Flags ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( HostVa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	// Map the memory range into the guest physical address space
	//
	return ::WHvMapGpaRange( Partition->Handle, HostVa, static_cast< WHV_GUEST_PHYSICAL_ADDRESS >( GuestPa ), ALIGN_UP( Size ), Flags );
}

/**
 * @brief Allocate memory in guest virtual address space (backed by host memory)
 *
 * Allocate memory in guest virtual address space (backed by host memory), mapping twice
 * on the same guest physical memory address will replace any existing physical memory mapping
 * but will not free the host virtual memory nor update the guest PTEs.
 * 
 * @param Partition The VM partition
 * @param HostVa The host virtual memory address backing the guest physical memory
 * @param GuestVa The guest virtual memory address
 * @param Size The size of the allocated memory
 * @param Flags The flags that describe the allocated memory (Read Write Execute)
 * @return A result code
 */
HRESULT WhSeAllocateGuestVirtualMemory( WHSE_PARTITION* Partition, PVOID* HostVa, uintptr_t GuestVa, size_t* Size, WHSE_MEMORY_ACCESS_FLAGS Flags ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( HostVa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( *HostVa != nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto size = ALIGN_UP( *Size );

	// Allocate memory into host
	//
	auto protectionFlags = AccessFlagsToProtectionFlags( Flags );
	auto allocatedHostVa = ::VirtualAlloc( nullptr, size, MEM_COMMIT | MEM_RESERVE, protectionFlags );
	if ( allocatedHostVa == nullptr )
		return WhSeGetLastHresult();

	auto hresult = S_OK;

	// Compute starting and ending guest virtual addresses
	//
	auto startingGva = ALIGN( GuestVa );
	auto endingGva = ALIGN_UP( startingGva + size );

	// Setup matching PTEs
	//
	for ( auto gva = startingGva; gva < endingGva; gva += PAGE_SIZE ) {
		hresult = WhSiInsertPageTableEntry( Partition, gva );
		if ( FAILED( hresult ) )
			return hresult;
	}

	uintptr_t gpa { };
	hresult = WhSeTranslateGvaToGpa( Partition, startingGva, &gpa, nullptr );
	if ( FAILED( hresult ) )
		return hresult;

	hresult = ::WHvMapGpaRange( Partition->Handle, allocatedHostVa, static_cast< WHV_GUEST_PHYSICAL_ADDRESS >( gpa ), size, Flags );
	if ( FAILED( hresult ) ) {
		if ( allocatedHostVa != nullptr )
			::VirtualFree( allocatedHostVa, 0, MEM_RELEASE );

		return hresult;
	}

	WHSE_ALLOCATION_NODE node {
		.GuestVirtualAddress = reinterpret_cast< PVOID >( GuestVa ),
		.GuestPhysicalAddress = gpa,
		.HostVirtualAddress = allocatedHostVa,
		.Size = size
	};

	hresult = WhSeInsertAllocationTrackingNode( Partition, node );
	if ( FAILED( hresult ) )
		return hresult;

	*HostVa = allocatedHostVa;
	*Size = size;

	return hresult;
}

/**
 * @brief Map memory from host to guest virtual address space (backed by host memory)
 *
 * Map host memory to guest virtual memory, mapping twice
 * on the same guest physical memory address will replace any existing physical memory mapping
 * but will not free the host virtual memory nor update the guest PTEs.
 * 
 * @param Partition The VM partition
 * @param HostVa The host virtual memory address backing the guest physical memory
 * @param GuestVa The guest virtual memory address
 * @param Size The size of the allocated memory
 * @param Flags The flags that describe the allocated memory (Read Write Execute)
 * @return A result code
 */
HRESULT WHSEAPI WhSeMapHostToGuestVirtualMemory( WHSE_PARTITION* Partition, PVOID HostVa, uintptr_t GuestVa, size_t Size, WHSE_MEMORY_ACCESS_FLAGS Flags ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( HostVa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto hresult = S_OK;

	// Compute starting and ending guest virtual addresses
	//
	auto startingGva = ALIGN( GuestVa );
	auto endingGva = ALIGN_UP( startingGva + Size );

	// Setup matching PTEs (and allocate guest physical memory accordingly)
	//
	for ( auto gva = startingGva; gva < endingGva; gva += PAGE_SIZE ) {
		hresult = WhSiInsertPageTableEntry( Partition, gva );
		if ( FAILED( hresult ) )
			return hresult;
	}

	uintptr_t gpa { };
	hresult = WhSeTranslateGvaToGpa( Partition, startingGva, &gpa, nullptr );
	if ( FAILED( hresult ) )
		return hresult;

	return ::WHvMapGpaRange( Partition->Handle, HostVa, static_cast< WHV_GUEST_PHYSICAL_ADDRESS >( gpa ), ALIGN_UP( Size ), Flags );
}

/**
 * @brief Free memory in guest physical address space
 *
 * @param Partition The VM partition
 * @param HostVa The host memory virtual address backing the physical guest memory
 * @param GuestPa The guest memory physical address
 * @param Size The size of the allocation
 * @return A result code
 */
HRESULT WHSEAPI WhSeFreeGuestPhysicalMemory( WHSE_PARTITION* Partition, PVOID HostVa, uintptr_t GuestPa, size_t Size ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( HostVa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto size = ALIGN_UP( Size );

	auto hresult = ::WHvUnmapGpaRange( Partition->Handle, static_cast< WHV_GUEST_PHYSICAL_ADDRESS >( GuestPa ), size );
	if ( FAILED( hresult ) )
		return hresult;

	auto result = ::VirtualFree( HostVa, 0, MEM_RELEASE );
	if ( !result )
		return WhSeGetLastHresult();

	return hresult;
}

/**
 * @brief Free memory in guest virtual address space
 *
 * @param Partition The VM partition
 * @param HostVa The host memory virtual address backing the physical guest memory
 * @param GuestVa The guest memory virtual address
 * @param Size The size of the allocation
 * @return A result code
 */
HRESULT WHSEAPI WhSeFreeGuestVirtualMemory( WHSE_PARTITION* Partition, PVOID HostVa, uintptr_t GuestVa, size_t Size ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( HostVa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto size = ALIGN_UP( Size );

	uintptr_t gpa { };
	auto hresult = WhSeTranslateGvaToGpa( Partition, GuestVa, &gpa, nullptr );
	if ( FAILED( hresult ) )
		return hresult;

	hresult = ::WHvUnmapGpaRange( Partition->Handle, static_cast< WHV_GUEST_PHYSICAL_ADDRESS >( gpa ), size );
	if ( FAILED( hresult ) )
		return hresult;

	auto result = ::VirtualFree( HostVa, 0, MEM_RELEASE );
	if ( !result )
		return WhSeGetLastHresult();

	return hresult;
}

/**
 * @brief Initialize paging and other memory stuff for the partition
 *
 * @param Partition The VM partition
 * @return A result code
 */
HRESULT WhSeInitializeMemoryLayout( WHSE_PARTITION* Partition ) {
	// Get available physical memory
	//
	uint64_t totalMemInKib = 0;
	if ( ::GetPhysicallyInstalledSystemMemory( &totalMemInKib ) == FALSE && totalMemInKib == 0 )
		return WhSeGetLastHresult();

	Partition->MemoryLayout.PhysicalAddressSpace.LowestAddress = 0x00000000'00000000;
	Partition->MemoryLayout.PhysicalAddressSpace.HighestAddress = ( totalMemInKib << 10 ) - 1;
	Partition->MemoryLayout.PhysicalAddressSpace.SizeInBytes = totalMemInKib << 10;

	uintptr_t pml4Address = 0;
	Partition->MemoryLayout.Pml4HostVa = nullptr;

	// Build paging tables
	//
	auto hresult = WhSiSetupPaging( Partition, &pml4Address );
	if ( FAILED( hresult ) )
		return hresult;

	Partition->MemoryLayout.Pml4PhysicalAddress = pml4Address;

	auto registers = Partition->VirtualProcessor.Registers;
	hresult = WhSeGetProcessorRegisters( Partition, registers );
	if ( FAILED( hresult ) )
		return hresult;
	
	// Enable paging and protected mode
	//
	registers[ Cr0 ].Reg64 = ( registers[ Cr0 ].Reg64 | ( 1ULL << 31 ) | ( 1 << 0 ) ) & UINT32_MAX;
	
	// Set the pml4 physical address
	//
	registers[ Cr3 ].Reg64 = pml4Address;
	
	// Enable PAE
	//
	registers[ Cr4 ].Reg64 = ( registers[ Cr4 ].Reg64 | ( 1ULL << 5 ) ) & ~( 1 << 24 );

	// Enable Long Mode
	//
	registers[ Efer ].Reg64 = ( registers[ Efer ].Reg64 | ( 1ULL << 8 ) ) & ~( 1 << 16 );

	// Update registers at this point
	//
	hresult = WhSeSetProcessorRegisters( Partition, registers );
	if ( FAILED( hresult ) )
		return hresult;

	// Setup custom IDT
	//
	hresult = WhSiSetupInterruptDescriptorTable( Partition, registers );
	if ( FAILED( hresult ) )
		return hresult;

	return WhSeSetProcessorRegisters( Partition, registers );
}

/**
 * @brief Translate guest virtual address to guest physical address
 *
 * @param Partition The VM partition
 * @param VirtualAddress The guest virtual address to be translated
 * @param PhysicalAddress The guest physical address backing the guest virtual address
 * @param TranslationResult The translation result
 * @return A result code
 */
HRESULT WhSeTranslateGvaToGpa( WHSE_PARTITION* Partition, uintptr_t VirtualAddress, uintptr_t* PhysicalAddress, WHV_TRANSLATE_GVA_RESULT* TranslationResult ) {
	if ( PhysicalAddress == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );
	
	WHV_GUEST_PHYSICAL_ADDRESS gpa;
	WHV_TRANSLATE_GVA_RESULT translationResult { };

	WHV_TRANSLATE_GVA_FLAGS flags = WHvTranslateGvaFlagValidateRead | WHvTranslateGvaFlagValidateWrite /*| WHvTranslateGvaFlagPrivilegeExempt*/;

	auto hresult = S_OK;
	auto tries = 0;
	
	do {
		hresult = ::WHvTranslateGva(
			Partition->Handle,
			Partition->VirtualProcessor.Index,
			VirtualAddress,
			flags,
			&translationResult,
			&gpa
		);

		if ( FAILED( hresult ) )
			return hresult;

		switch ( translationResult.ResultCode )
		{
			case WHvTranslateGvaResultPageNotPresent:
				hresult = WhSiInsertPageTableEntry( Partition, VirtualAddress );
				break;
			case WHvTranslateGvaResultPrivilegeViolation:
			case WHvTranslateGvaResultInvalidPageTableFlags:
			case WHvTranslateGvaResultGpaUnmapped:
			case WHvTranslateGvaResultGpaNoReadAccess:
			case WHvTranslateGvaResultGpaNoWriteAccess:
			case WHvTranslateGvaResultGpaIllegalOverlayAccess:
			case WHvTranslateGvaResultIntercept:
				DebugBreak();
				break;
			case WHvTranslateGvaResultSuccess:
				break;
			default:
				break;
		}

		tries++;
	} while ( tries < 2 && hresult == S_OK && translationResult.ResultCode != WHvTranslateGvaResultSuccess );

	if ( FAILED( hresult ) )
		return hresult;

	*PhysicalAddress = gpa;

	if ( TranslationResult != nullptr )
		*TranslationResult = translationResult;

	return hresult;
}