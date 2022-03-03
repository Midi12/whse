#include "WinHvMemory.hpp"
#include "WinHvMemoryInternal.hpp"
#include "WinHvHelpers.hpp"
#include "WinHvProcessor.hpp"
#include "WinHvUtils.hpp"

constexpr uint32_t AccessFlagsToProtectionFlags( WHSE_MEMORY_ACCESS_FLAGS flags ) {
	uint32_t protectionFlags = 0;

	if ( flags == WHvMapGpaRangeFlagNone )
		return PAGE_NOACCESS;

	if ( HAS_FLAGS( flags, WHvMapGpaRangeFlagRead ) )
		protectionFlags |= ( 1 << 1 );

	if ( HAS_FLAGS( flags, WHvMapGpaRangeFlagWrite ) )
		protectionFlags <<= 1;

	if ( HAS_FLAGS( flags, WHvMapGpaRangeFlagExecute ) )
		protectionFlags <<= 4;

	return protectionFlags;
}

// Allocate memory in guest physical address space (backed by host memory)
//
HRESULT WhSeAllocateGuestPhysicalMemory( WHSE_PARTITION* Partition, PVOID* HostVa, uintptr_t GuestPa, size_t* Size, WHSE_MEMORY_ACCESS_FLAGS Flags ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( HostVa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( *HostVa != nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto size = ALIGN_PAGE( *Size );

	// Allocate memory into host
	//
	auto protectionFlags = AccessFlagsToProtectionFlags( Flags );
	auto allocatedGuestVa = ::VirtualAlloc( nullptr, size, MEM_COMMIT | MEM_RESERVE, protectionFlags );
	if ( allocatedGuestVa == nullptr )
		return WhSeGetLastHresult();

	*HostVa = allocatedGuestVa;
	*Size = size;

	// Create the allocated range into the guest physical address space
	//
	return ::WHvMapGpaRange( Partition->Handle, allocatedGuestVa, static_cast< WHV_GUEST_PHYSICAL_ADDRESS >( GuestPa ), size, Flags );
}

// Map memory from host to guest physical address space (backed by host memory)
//
HRESULT WHSEAPI WhSeMapHostToGuestPhysicalMemory( WHSE_PARTITION* Partition, PVOID HostVa, uintptr_t GuestPa, size_t Size, WHSE_MEMORY_ACCESS_FLAGS Flags ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( HostVa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	// Map the memory range into the guest physical address space
	//
	return ::WHvMapGpaRange( Partition->Handle, HostVa, static_cast< WHV_GUEST_PHYSICAL_ADDRESS >( GuestPa ), ALIGN_PAGE( Size ), Flags );
}

// Allocate memory in guest virtual address space (backed by host memory)
//
HRESULT WhSeAllocateGuestVirtualMemory( WHSE_PARTITION* Partition, PVOID* HostVa, uintptr_t GuestVa, size_t* Size, WHSE_MEMORY_ACCESS_FLAGS Flags ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( HostVa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( *HostVa != nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto size = ALIGN_PAGE( *Size );

	// Allocate memory into host
	//
	auto protectionFlags = AccessFlagsToProtectionFlags( Flags );
	auto allocatedGuestVa = ::VirtualAlloc( nullptr, size, MEM_COMMIT | MEM_RESERVE, protectionFlags );
	if ( allocatedGuestVa == nullptr )
		return WhSeGetLastHresult();

	*HostVa = allocatedGuestVa;
	*Size = size;

	auto hresult = S_OK;

	// No guest VA specified, let the system choose it
	//
	if ( GuestVa == 0) {
		if ( FAILED( hresult = WhSiFindBestGVA( Partition, &GuestVa, size ) ) )
			return hresult;
	}

	uintptr_t gpa { };
	WHV_TRANSLATE_GVA_RESULT translateResult { };
	hresult = WhSeTranslateVirtualAddress( Partition, GuestVa, &gpa, &translateResult );
	if ( FAILED( hresult ) )
		return hresult;

	// Create the allocated range into the guest address space
	//
	return ::WHvMapGpaRange( Partition->Handle, allocatedGuestVa, static_cast< WHV_GUEST_PHYSICAL_ADDRESS >( gpa ), size, Flags );
}

// Map memory from host to guest virtual address space (backed by host memory)
//
HRESULT WHSEAPI WhSeMapHostToGuestVirtualMemory( WHSE_PARTITION* Partition, PVOID HostVa, uintptr_t GuestVa, size_t Size, WHSE_MEMORY_ACCESS_FLAGS Flags ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( HostVa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	uintptr_t gpa { };
	WHV_TRANSLATE_GVA_RESULT translateResult { };
	auto hresult = WhSeTranslateVirtualAddress( Partition, GuestVa, &gpa, &translateResult );
	if ( FAILED( hresult ) )
		return hresult;

	// Map the memory range into the guest address space
	//
	return ::WHvMapGpaRange( Partition->Handle, HostVa, static_cast< WHV_GUEST_PHYSICAL_ADDRESS >( gpa ), ALIGN_PAGE( Size ), Flags );
}

// Free memory in guest address space
//
HRESULT WhSeFreeGuestMemory( WHSE_PARTITION* Partition, PVOID HostVa, uintptr_t GuestVa, size_t Size ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( HostVa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto size = ALIGN_PAGE( Size );

	auto hresult = ::WHvUnmapGpaRange( Partition->Handle, static_cast< WHV_GUEST_PHYSICAL_ADDRESS >( GuestVa ), size );
	if ( FAILED( hresult ) )
		return hresult;

	auto result = ::VirtualFree( HostVa, 0, MEM_RELEASE );
	if ( !result )
		return WhSeGetLastHresult();

	return hresult;
}

// Copy memory from host address space to guest address space
//
HRESULT WhSeCopyHostToGuestMemory( WHSE_PARTITION* Partition, PVOID GuestDestVa, const void* HostSourceVa, size_t Size ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( HostSourceVa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	// Map the source data into the guest address space
	//
	return ::WHvMapGpaRange( Partition->Handle, const_cast< void* >( HostSourceVa ), reinterpret_cast< WHV_GUEST_PHYSICAL_ADDRESS >( GuestDestVa ), Size, WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite | WHvMapGpaRangeFlagExecute );
}

// Initialize paging and other memory stuff for the partition
//
HRESULT WhSeInitializeMemoryLayout( WHSE_PARTITION* Partition ) {
	// Get available physical memory
	//
	uint64_t totalMemInKib = 0;
	if ( ::GetPhysicallyInstalledSystemMemory( &totalMemInKib ) == FALSE && totalMemInKib == 0 )
		return WhSeGetLastHresult();

	Partition->MemoryLayout.LowestAddress = 0x00000000'00000000;
	Partition->MemoryLayout.HighestAddress = ( totalMemInKib << 10 ) - 1;
	Partition->MemoryLayout.SizeInBytes = totalMemInKib << 10;

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

	return WhSeSetProcessorRegisters( Partition, registers );
}

// Translate virtual address to physical address
//
HRESULT WhSeTranslateVirtualAddress( WHSE_PARTITION* Partition, uintptr_t VirtualAddress, uintptr_t* PhysicalAddress, WHV_TRANSLATE_GVA_RESULT* TranslationResult ) {
	if ( PhysicalAddress == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );
	
	WHV_GUEST_PHYSICAL_ADDRESS gpa;
	WHV_TRANSLATE_GVA_RESULT translationResult { };

	WHV_TRANSLATE_GVA_FLAGS flags = WHvTranslateGvaFlagValidateRead | WHvTranslateGvaFlagValidateWrite /*| WHvTranslateGvaFlagPrivilegeExempt*/;

	auto hresult = S_OK;
	
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
	} while ( hresult == S_OK && translationResult.ResultCode != WHvTranslateGvaResultSuccess );

	if ( FAILED( hresult ) )
		return hresult;

	*PhysicalAddress = gpa;

	if ( TranslationResult != nullptr )
		*TranslationResult = translationResult;

	return hresult;
}

// Page fault handler
//
HRESULT WHSEAPI WhSePageFaultHandler( WHSE_PARTITION* Partition ) {
	return S_OK;
}