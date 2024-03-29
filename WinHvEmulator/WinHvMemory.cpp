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
HRESULT WhSeAllocateGuestPhysicalMemory( WHSE_PARTITION* Partition, uintptr_t* HostVa, uintptr_t* GuestPa, size_t Size, WHSE_MEMORY_ACCESS_FLAGS Flags ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( HostVa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( *HostVa != 0 )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( GuestPa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto size = ALIGN_UP( Size );

	uintptr_t suggestedGpa = 0;
	if ( *GuestPa == 0 ) {
		auto hresult = WhSiSuggestPhysicalAddress( Partition, size, &suggestedGpa );
		if ( FAILED( hresult ) )
			return hresult;
	}
	else
		suggestedGpa = ALIGN( *GuestPa );

	PWHSE_ALLOCATION_NODE existingNode = nullptr;
	auto hresult = WhSeFindAllocationNodeByGpa( Partition, suggestedGpa, &existingNode );
	if ( hresult != HRESULT_FROM_WIN32( ERROR_NOT_FOUND ) && FAILED( hresult ) )
		return hresult;

	if ( existingNode != nullptr )
		return HRESULT_FROM_WIN32( ERROR_INTERNAL_ERROR );

	// Allocate memory into host
	//
	auto protectionFlags = AccessFlagsToProtectionFlags( Flags );
	auto allocatedHostVa = ::VirtualAlloc( nullptr, Size, MEM_COMMIT | MEM_RESERVE, protectionFlags );
	if ( allocatedHostVa == nullptr )
		return WhSeGetLastHresult();

	WHSE_ALLOCATION_NODE node {
		.BlockType = MEMORY_BLOCK_TYPE::MemoryBlockPhysical,
		.HostVirtualAddress = reinterpret_cast< uintptr_t >( allocatedHostVa ),
		.GuestPhysicalAddress = suggestedGpa,
		.GuestVirtualAddress = 0,
		.Size = size
	};

	hresult = WhSeInsertAllocationTrackingNode( Partition, node );
	if ( FAILED( hresult ) )
		return hresult;

	// Create the allocated range into the guest physical address space
	//
	hresult = ::WHvMapGpaRange( Partition->Handle, allocatedHostVa, static_cast< WHV_GUEST_PHYSICAL_ADDRESS >( suggestedGpa ), size, Flags );
	if ( FAILED( hresult ) ) {
		if ( allocatedHostVa != nullptr )
			::VirtualFree( allocatedHostVa, 0, MEM_RELEASE );

		return hresult;
	}

	*HostVa = reinterpret_cast< uintptr_t >( allocatedHostVa );
	*GuestPa = suggestedGpa;

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
HRESULT WHSEAPI WhSeMapHostToGuestPhysicalMemory( WHSE_PARTITION* Partition, uintptr_t HostVa, uintptr_t* GuestPa, size_t Size, WHSE_MEMORY_ACCESS_FLAGS Flags ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( HostVa == 0 )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( GuestPa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto size = ALIGN_UP( Size );

	uintptr_t suggestedGpa = 0;
	if ( *GuestPa == 0 ) {
		auto hresult = WhSiSuggestPhysicalAddress( Partition, size, &suggestedGpa );
		if ( FAILED( hresult ) )
			return hresult;
	}
	else
		suggestedGpa = ALIGN( *GuestPa );

	PWHSE_ALLOCATION_NODE existingNode = nullptr;
	auto hresult = WhSeFindAllocationNodeByGpa( Partition, suggestedGpa, &existingNode );
	if ( hresult != HRESULT_FROM_WIN32( ERROR_NOT_FOUND ) && FAILED( hresult ) )
		return hresult;

	if ( existingNode != nullptr )
		return HRESULT_FROM_WIN32( ERROR_INTERNAL_ERROR );

	WHSE_ALLOCATION_NODE node {
		.BlockType = MEMORY_BLOCK_TYPE::MemoryBlockPhysical,
		.HostVirtualAddress = HostVa,
		.GuestPhysicalAddress = suggestedGpa,
		.GuestVirtualAddress = 0,
		.Size = size
	};

	hresult = WhSeInsertAllocationTrackingNode( Partition, node );
	if ( FAILED( hresult ) )
		return hresult;

	// Map the memory range into the guest physical address space
	//
	hresult = ::WHvMapGpaRange( Partition->Handle, reinterpret_cast< PVOID >( HostVa ), static_cast< WHV_GUEST_PHYSICAL_ADDRESS >( suggestedGpa ), size, Flags );
	if ( FAILED( hresult ) )
		return hresult;

	*GuestPa = suggestedGpa;

	return hresult;
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
HRESULT WhSeAllocateGuestVirtualMemory( WHSE_PARTITION* Partition, uintptr_t* HostVa, uintptr_t* GuestVa, size_t Size, WHSE_MEMORY_ACCESS_FLAGS Flags ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( HostVa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( *HostVa != 0 )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( GuestVa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto size = ALIGN_UP( Size );

	PWHSE_ALLOCATION_NODE existingNode = nullptr;
	auto hresult = WhSeFindAllocationNodeByGva( Partition, *GuestVa, &existingNode );
	if ( hresult != HRESULT_FROM_WIN32( ERROR_NOT_FOUND ) && FAILED( hresult ) )
		return hresult;

	uintptr_t suggestedGva = 0;
	if ( *GuestVa == 0 || existingNode != nullptr) {
		auto hresult = WhSiSuggestVirtualAddress( Partition, size, &suggestedGva, Partition->VirtualProcessor.Mode );
		if ( FAILED( hresult ) )
			return hresult;
	}
	else
		suggestedGva = ALIGN( *GuestVa );

	existingNode = nullptr;
	hresult = WhSeFindAllocationNodeByGva( Partition, suggestedGva, &existingNode );
	if ( hresult != HRESULT_FROM_WIN32( ERROR_NOT_FOUND ) && FAILED( hresult ) )
		return hresult;

	if ( existingNode != nullptr )
		return HRESULT_FROM_WIN32( ERROR_INTERNAL_ERROR );

	// Compute starting and ending guest virtual addresses
	//
	auto startingGva = ALIGN( suggestedGva );
	auto endingGva = ALIGN_UP( startingGva + size );

	// Allocate memory into host
	//
	auto protectionFlags = AccessFlagsToProtectionFlags( Flags );
	auto allocatedHostVa = ::VirtualAlloc( nullptr, size, MEM_COMMIT | MEM_RESERVE, protectionFlags );
	if ( allocatedHostVa == nullptr )
		return WhSeGetLastHresult();

	uintptr_t suggestedGpa = 0;
	hresult = WhSiSuggestPhysicalAddress( Partition, size, &suggestedGpa );
	if ( FAILED( hresult ) )
		return hresult;

	WHSE_ALLOCATION_NODE node {
		.BlockType = MEMORY_BLOCK_TYPE::MemoryBlockVirtual,
		.HostVirtualAddress = reinterpret_cast< uintptr_t >( allocatedHostVa ),
		.GuestPhysicalAddress = suggestedGpa,
		.GuestVirtualAddress = startingGva,
		.Size = size
	};

	hresult = WhSeInsertAllocationTrackingNode( Partition, node );
	if ( FAILED( hresult ) )
		return hresult;

	// Setup matching PTEs
	//
	for ( auto gva = startingGva, page = suggestedGpa; gva < endingGva; gva += PAGE_SIZE, page += PAGE_SIZE ) {
		hresult = WhSiInsertPageTableEntry( Partition, gva, page);
		if ( FAILED( hresult ) )
			return hresult;
	}

	hresult = ::WHvMapGpaRange( Partition->Handle, allocatedHostVa, static_cast< WHV_GUEST_PHYSICAL_ADDRESS >( suggestedGpa ), size, Flags );
	if ( FAILED( hresult ) ) {
		if ( allocatedHostVa != nullptr )
			::VirtualFree( allocatedHostVa, 0, MEM_RELEASE );

		return hresult;
	}

	*HostVa = reinterpret_cast< uintptr_t >( allocatedHostVa );
	*GuestVa = startingGva;

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
HRESULT WHSEAPI WhSeMapHostToGuestVirtualMemory( WHSE_PARTITION* Partition, uintptr_t HostVa, uintptr_t* GuestVa, size_t Size, WHSE_MEMORY_ACCESS_FLAGS Flags ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( HostVa == 0 )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( GuestVa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto size = ALIGN_UP( Size );

	PWHSE_ALLOCATION_NODE existingNode = nullptr;
	auto hresult = WhSeFindAllocationNodeByGva( Partition, *GuestVa, &existingNode );
	if ( hresult != HRESULT_FROM_WIN32( ERROR_NOT_FOUND ) && FAILED( hresult ) )
		return hresult;

	uintptr_t suggestedGva = 0;
	if ( *GuestVa == 0 || existingNode != nullptr ) {
		auto hresult = WhSiSuggestVirtualAddress( Partition, size, &suggestedGva, Partition->VirtualProcessor.Mode );
		if ( FAILED( hresult ) )
			return hresult;
	}
	else
		suggestedGva = ALIGN( *GuestVa );

	existingNode = nullptr;
	hresult = WhSeFindAllocationNodeByGva( Partition, suggestedGva, &existingNode );
	if ( hresult != HRESULT_FROM_WIN32( ERROR_NOT_FOUND ) && FAILED( hresult ) )
		return hresult;

	if ( existingNode != nullptr )
		return HRESULT_FROM_WIN32( ERROR_INTERNAL_ERROR );

	// Compute starting and ending guest virtual addresses
	//
	auto startingGva = ALIGN( suggestedGva );
	auto endingGva = ALIGN_UP( startingGva + size );

	uintptr_t suggestedGpa = 0;
	hresult = WhSiSuggestPhysicalAddress( Partition, size, &suggestedGpa );
	if ( FAILED( hresult ) )
		return hresult;

	WHSE_ALLOCATION_NODE node {
		.BlockType = MEMORY_BLOCK_TYPE::MemoryBlockVirtual,
		.HostVirtualAddress = HostVa,
		.GuestPhysicalAddress = suggestedGpa,
		.GuestVirtualAddress = startingGva,
		.Size = size
	};

	hresult = WhSeInsertAllocationTrackingNode( Partition, node );
	if ( FAILED( hresult ) )
		return hresult;

	// Setup matching PTEs
	//
	for ( auto gva = startingGva, page = suggestedGpa; gva < endingGva; gva += PAGE_SIZE, page += PAGE_SIZE ) {
		hresult = WhSiInsertPageTableEntry( Partition, gva, page );
		if ( FAILED( hresult ) )
			return hresult;
	}

	hresult = ::WHvMapGpaRange( Partition->Handle, reinterpret_cast< PVOID >( HostVa ), static_cast< WHV_GUEST_PHYSICAL_ADDRESS >( suggestedGpa ), size, Flags );
	if ( FAILED( hresult ) )
		return hresult;

	*GuestVa = startingGva;

	return hresult;
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
HRESULT WHSEAPI WhSeFreeGuestPhysicalMemory( WHSE_PARTITION* Partition, uintptr_t HostVa, uintptr_t GuestPa, size_t Size ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( HostVa == 0 )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	/*WHSE_ALLOCATION_NODE* node = nullptr;
	auto hresult = WhSeFindAllocationNodeByGpa( Partition, GuestPa, &node );
	if ( FAILED( hresult ) )
		return hresult;*/

	auto hresult = ::WHvUnmapGpaRange( Partition->Handle, static_cast< WHV_GUEST_PHYSICAL_ADDRESS >( GuestPa ), ALIGN_UP( Size ) );
	if ( FAILED( hresult ) )
		return hresult;

	auto result = ::VirtualFree( reinterpret_cast< PVOID >( HostVa ), 0, MEM_RELEASE );
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
HRESULT WHSEAPI WhSeFreeGuestVirtualMemory( WHSE_PARTITION* Partition, uintptr_t HostVa, uintptr_t GuestVa, size_t Size ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( HostVa == 0 )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	/*WHSE_ALLOCATION_NODE* node = nullptr;
	auto hresult = WhSeFindAllocationNodeByGva( Partition, GuestVa, &node );
	if ( FAILED( hresult ) )
		return hresult;*/

	uintptr_t gpa { };
	auto hresult = WhSeTranslateGvaToGpa( Partition, GuestVa, &gpa, nullptr );
	if ( FAILED( hresult ) )
		return hresult;

	hresult = ::WHvUnmapGpaRange( Partition->Handle, static_cast< WHV_GUEST_PHYSICAL_ADDRESS >( gpa ), ALIGN_UP( Size ) );
	if ( FAILED( hresult ) )
		return hresult;

	auto result = ::VirtualFree( reinterpret_cast< PVOID >( HostVa ), 0, MEM_RELEASE );
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
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto hresult = WhSiInitializeMemoryArena( Partition );
	if ( FAILED( hresult ) )
		return hresult;

	uintptr_t pml4Address = 0;
	Partition->MemoryLayout.Pml4HostVa = 0;

	// Build paging tables
	//
	hresult = WhSiSetupPaging( Partition, &pml4Address );
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

	// Enable Long Mode and Syscall
	//
	registers[ Efer ].Reg64 = ( registers[ Efer ].Reg64 | ( 1ULL << 0 ) | ( 1ULL << 8 ) ) & ~( 1 << 16 );

	// Update registers at this point
	//
	hresult = WhSeSetProcessorRegisters( Partition, registers );
	if ( FAILED( hresult ) )
		return hresult;

	// Setup GDT
	//
	hresult = WhSiSetupGlobalDescriptorTable( Partition, registers );
	if ( FAILED( hresult ) )
		return hresult;

	// Setup IDT
	//
	hresult = WhSiSetupInterruptDescriptorTable( Partition, registers );
	if ( FAILED( hresult ) )
		return hresult;

	// Setup Syscall
	//
	hresult = WhSiSetupSyscalls( Partition, registers );
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

	WHV_TRANSLATE_GVA_FLAGS flags = WHvTranslateGvaFlagValidateRead | WHvTranslateGvaFlagValidateWrite | WHvTranslateGvaFlagPrivilegeExempt;

	auto hresult = ::WHvTranslateGva(
		Partition->Handle,
		Partition->VirtualProcessor.Index,
		VirtualAddress,
		flags,
		&translationResult,
		&gpa
	);
	if ( FAILED( hresult ) )
		return hresult;

	if ( TranslationResult != nullptr )
		*TranslationResult = translationResult;

	*PhysicalAddress = gpa;

	return hresult;
}