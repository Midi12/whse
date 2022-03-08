#include "WinHvMemoryInternal.hpp"
#include "WinHvMemory.hpp"
#include "WinHvUtils.hpp"
#include "winbase.h"
#include "winerror.h"
#include "winnt.h"

#include <cstdint>
#include <windows.h>


HRESULT WhSiDecomposeVirtualAddress( uintptr_t VirtualAddress, uint16_t* Pml4Index, uint16_t* PdpIndex, uint16_t* PdIndex, uint16_t* PtIndex, uint16_t* Offset ) {

	*Offset = VirtualAddress & 0xFFF;
	*PtIndex = ( VirtualAddress >> 12 ) & 0x1FF;
	*PdIndex = ( VirtualAddress >> ( 12 + 9 ) ) & 0x1FF;
	*PdpIndex = ( VirtualAddress >> ( 12 + 9 * 2 ) ) & 0x1FF;
	*Pml4Index = ( VirtualAddress >> ( 12 + 9 * 3 ) ) & 0x1FF;

	return S_OK;
}

static uintptr_t s_nextPhysicalAddress = 0x00000000'00000000;

// Get one or more physical page
//
HRESULT WhSiGetNextPhysicalPages( WHSE_PARTITION* Partition, size_t NumberOfPages, uintptr_t* PhysicalPageAddress ) {
	if ( NumberOfPages <= 0 )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( PhysicalPageAddress == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	uintptr_t physicalAddress = s_nextPhysicalAddress;

	// this is a very naive approach but okay for our small emulation goal
	//
	if ( physicalAddress >= Partition->MemoryLayout.PhysicalAddressSpace.HighestAddress )
		return HRESULT_FROM_WIN32( ERROR_OUTOFMEMORY );

	s_nextPhysicalAddress += ( PAGE_SIZE * NumberOfPages );
	*PhysicalPageAddress = physicalAddress;

	return S_OK;
}

// Get one physical page
//
HRESULT WhSiGetNextPhysicalPage( WHSE_PARTITION* Partition, uintptr_t* PhysicalPageAddress ) {
	return WhSiGetNextPhysicalPages( Partition, 1, PhysicalPageAddress );
}

// Internal helper to allocate host memory to guest physical memory
//
HRESULT WhSiAllocateGuestPhysicalMemory( WHSE_PARTITION* Partition, PVOID* HostVa, uintptr_t* GuestPa, size_t* Size, WHSE_MEMORY_ACCESS_FLAGS Flags ) {
	uintptr_t physicalAddress = 0;
	auto hresult = WhSiGetNextPhysicalPage( Partition, &physicalAddress );
	if ( FAILED( hresult ) )
		return hresult;

	PVOID hostVa = nullptr;
	size_t size = ALIGN_PAGE( *Size );
	hresult = WhSeAllocateGuestPhysicalMemory( Partition, &hostVa, physicalAddress, &size, Flags );
	if ( FAILED( hresult ) )
		return hresult;

	*HostVa = hostVa;
	*GuestPa = physicalAddress;
	*Size = size;

	return S_OK;
}

HRESULT WhSpInsertPageTableEntry( WHSE_PARTITION* Partition, PMMPTE_HARDWARE ParentLayer, uint16_t Index ) {
	uintptr_t gpa = 0;
	PVOID hva = nullptr;
	size_t size = PAGE_SIZE;

	auto hresult = WhSiAllocateGuestPhysicalMemory( Partition, &hva, &gpa, &size, WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite );
	if ( FAILED( hresult ) )
		return hresult;

	// Create a valid PTE
	//
	MMPTE_HARDWARE pte { };

	pte.AsUlonglong = 0;							// Ensure zeroed
	pte.Valid = 1;									// Intel's Present bit
	pte.Write = 1;									// Intel's Read/Write bit
	pte.Owner = 1;									// Intel's User/Supervisor bit, let's say it is a user accessible frame
	pte.PageFrameNumber = ( gpa / PAGE_SIZE );		// Physical address of PDP page

	ParentLayer[ Index ] = pte;

	WHSE_PFNDBNODE* node = reinterpret_cast< WHSE_PFNDBNODE* >( ::HeapAlloc( ::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( decltype( *node ) ) ) );
	if ( node == nullptr )
		return HRESULT_FROM_WIN32( ERROR_OUTOFMEMORY );

	node->PageFrameNumber = pte.PageFrameNumber;
	node->HostVa = hva;

	::InterlockedPushEntrySList( Partition->MemoryLayout.PageFrameNumberNodes, node );

	return hresult;
}

// Internal function to setup paging
//
HRESULT WhSiSetupPaging( WHSE_PARTITION* Partition, uintptr_t* Pml4PhysicalAddress ) {
	// Check if already initialized
	//
	if ( Partition->MemoryLayout.Pml4HostVa != nullptr )
		return HRESULT_FROM_WIN32( ERROR_ALREADY_INITIALIZED );

	PSLIST_HEADER pfnNodes = reinterpret_cast< PSLIST_HEADER >( ::HeapAlloc( ::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( decltype( *pfnNodes ) ) ) );
	if ( pfnNodes == nullptr )
		return HRESULT_FROM_WIN32( ERROR_OUTOFMEMORY );

	::InitializeSListHead( pfnNodes );
	Partition->MemoryLayout.PageFrameNumberNodes = pfnNodes;
	
	// Allocate PML4 on physical memory
	//
	uintptr_t pml4Gpa = 0;
	PVOID pml4Hva = nullptr;
	size_t size = PAGE_SIZE;
	auto hresult = WhSiAllocateGuestPhysicalMemory( Partition, &pml4Hva, &pml4Gpa, &size, WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite );
	if ( FAILED( hresult ) )
		return hresult;

	Partition->MemoryLayout.Pml4HostVa = pml4Hva;

	auto pml4 = reinterpret_cast< PMMPTE_HARDWARE >( pml4Hva );

	for ( auto i = 0; i < 512; i++ ) {
		// Allocate a Page Directory Pointers page
		// The memory is zeroed so every PDP entries will have the Valid (Present) bit set to 0
		//
		hresult = WhSpInsertPageTableEntry( Partition, pml4, i );
		if ( FAILED( hresult ) )
			return hresult;
	}

	*Pml4PhysicalAddress = pml4Gpa;

	// Initialize Guest Virtual Address Space allocations tracking
	//
	PSLIST_HEADER vaList = reinterpret_cast< PSLIST_HEADER >( ::HeapAlloc( ::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( decltype( *vaList ) ) ) );
	if ( vaList == nullptr )
		return HRESULT_FROM_WIN32( ERROR_OUTOFMEMORY );

	::InitializeSListHead( vaList );
	Partition->MemoryLayout.AllocatedVirtualSpaceNodes = vaList;
	
	return hresult;
}

HRESULT WhSpLookupHVAFromPFN( WHSE_PARTITION* Partition, uintptr_t PageFrameNumber, PVOID* HostVa ) {
	auto first = reinterpret_cast< WHSE_PFNDBNODE* >( ::RtlFirstEntrySList( Partition->MemoryLayout.PageFrameNumberNodes ) );
	if ( first == nullptr )
		return HRESULT_FROM_WIN32( ERROR_NO_MORE_ITEMS );

	auto current = first;
	while ( current != nullptr ) {
		if ( current->PageFrameNumber == PageFrameNumber ) {
			*HostVa = current->HostVa;
			break;
		}

		current = reinterpret_cast< WHSE_PFNDBNODE* >( current->Next );
	}

	return S_OK;
}

// Internal function to insert page table in the paging directory
// Allocate PML4 entry, PDP entry, PD entry and PT entry
//
HRESULT WhSiInsertPageTableEntry( WHSE_PARTITION* Partition, uintptr_t VirtualAddress ) {
	// "Explode" the VA into translation indexes
	//
	uint16_t pml4Idx;
	uint16_t pdpIdx;
	uint16_t pdIdx;
	uint16_t ptIdx;
	uint16_t phyOffset;

	auto hresult = WhSiDecomposeVirtualAddress( VirtualAddress, &pml4Idx, &pdpIdx, &pdIdx, &ptIdx, &phyOffset );
	if ( FAILED( hresult ) )
		return hresult;

	// Search entry in PML4
	// 
	auto pml4e = reinterpret_cast< PMMPTE_HARDWARE >( Partition->MemoryLayout.Pml4HostVa )[ pml4Idx ];
	if ( pml4e.Valid == FALSE ) {
		// Shouldn't happen as we initialized all PLM4 entries upfront
		//
		DebugBreak();
	}
	
	// Search entry in PDP
	// 
	PVOID pdpHva = nullptr;
	hresult = WhSpLookupHVAFromPFN( Partition, pml4e.PageFrameNumber, &pdpHva );
	if ( FAILED( hresult ) )
		return hresult;

	auto pdp = reinterpret_cast< PMMPTE_HARDWARE >( pdpHva );
	auto pdpe = pdp[ pdpIdx ];
	if ( pdpe.Valid == FALSE ) {
		// Allocate a Page Directory page
		//
		hresult = WhSpInsertPageTableEntry(
			Partition,
			pdp,
			pdpIdx
		);

		if ( FAILED( hresult ) )
			return hresult;

		pdpe = pdp[ pdpIdx ];
	}

	// Search entry in PD
	//
	PVOID pdHva = nullptr;
	hresult = WhSpLookupHVAFromPFN( Partition, pdpe.PageFrameNumber, &pdHva );
	if ( FAILED( hresult ) )
		return hresult;

	auto pd = reinterpret_cast< PMMPTE_HARDWARE >( pdHva );
	auto pde = pd[ pdIdx ];
	if ( pde.Valid == FALSE ) {
		// Allocate a Page Table page
		//
		hresult = WhSpInsertPageTableEntry(
			Partition,
			pd,
			pdIdx
		);

		if ( FAILED( hresult ) )
			return hresult;

		pde = pd[ pdIdx ];
	}

	// Add entry in PT
	//
	PVOID ptHva = nullptr;
	hresult = WhSpLookupHVAFromPFN( Partition, pde.PageFrameNumber, &ptHva );
	if ( FAILED( hresult ) )
		return hresult;

	auto pt = reinterpret_cast< PMMPTE_HARDWARE >( ptHva );
	auto pte = pt[ ptIdx ];
	if ( pte.Valid == FALSE ) {
		// Allocate a Page Table page
		//
		hresult = WhSpInsertPageTableEntry(
			Partition,
			pt,
			ptIdx
		);

		if ( FAILED( hresult ) )
			return hresult;

		pte = pt[ ptIdx ];
	}

	return S_OK;
}

// Find a suitable Guest VA
//
HRESULT WhSiFindBestGVA( WHSE_PARTITION* Partition, uintptr_t* GuestVa, size_t Size ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( GuestVa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto vaList = Partition->MemoryLayout.AllocatedVirtualSpaceNodes;
	if ( vaList == nullptr )
		return HRESULT_FROM_WIN32( PEERDIST_ERROR_NOT_INITIALIZED );

	auto first = reinterpret_cast< WHSE_VANODE* >( ::RtlFirstEntrySList( vaList ) );
	if ( first == nullptr )
		return HRESULT_FROM_WIN32( ERROR_NO_MORE_ITEMS );

	auto current = first;
	while ( current != nullptr ) {
		current = reinterpret_cast< WHSE_VANODE* >( current->Next );
	}

	auto va = ALIGN_PAGE( reinterpret_cast< uintptr_t >( current->VirtualAddress ) + current->Size );

	WHSE_VANODE* node = reinterpret_cast< WHSE_VANODE* >( ::HeapAlloc( ::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( decltype( *node ) ) ) );
	if ( node == nullptr )
		return HRESULT_FROM_WIN32( ERROR_OUTOFMEMORY );

	node->VirtualAddress = reinterpret_cast< PVOID >( va );
	node->Size = ALIGN_PAGE( Size );
	::InterlockedPushEntrySList( vaList, node );

	*GuestVa = va;

	return S_OK;
}