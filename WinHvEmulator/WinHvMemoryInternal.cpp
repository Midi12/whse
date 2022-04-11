#include "WinHvMemoryInternal.hpp"
#include "WinHvMemory.hpp"
#include "WinHvUtils.hpp"
#include "winbase.h"
#include "winerror.h"
#include "winnt.h"

#include <cstdint>
#include <windows.h>
#include "WinHvMemoryPrivate.hpp"
#include "WinHvAllocationTracker.hpp"

/**
 * @brief Break down a virtual address to paging indexes
 *
 * @param VirtualAddress The virtual address
 * @param Pml4Index The Page Map Level Four (PML4) index
 * @param PdpIndex The Page Directory Pointers index
 * @param PdIndex The Page Directory index
 * @param PtIndex The Page Table index
 * @param Offset The physical page offset
 * @return A result code
 */
HRESULT WhSiDecomposeVirtualAddress( uintptr_t VirtualAddress, uint16_t* Pml4Index, uint16_t* PdpIndex, uint16_t* PdIndex, uint16_t* PtIndex, uint16_t* Offset ) {

	*Offset = VirtualAddress & 0xFFF;
	*PtIndex = ( VirtualAddress >> 12 ) & 0x1FF;
	*PdIndex = ( VirtualAddress >> ( 12 + 9 ) ) & 0x1FF;
	*PdpIndex = ( VirtualAddress >> ( 12 + 9 * 2 ) ) & 0x1FF;
	*Pml4Index = ( VirtualAddress >> ( 12 + 9 * 3 ) ) & 0x1FF;

	return S_OK;
}

static uintptr_t s_nextPhysicalAddress = 0x00000000'00000000 + PAGE_SIZE;

/**
 * @brief Get one or more physical page
 *
 * @param Partition The VM partition
 * @param NumberOfPages The number of pages requested
 * @param PhysicalPageAddress The returned guest physical address
 * @return A result code
 */
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

/**
 * @brief Get one physical page
 *
 * @param Partition The VM partition
 * @param PhysicalPageAddress The returned guest physical address
 * @return A result code
 */
HRESULT WhSiGetNextPhysicalPage( WHSE_PARTITION* Partition, uintptr_t* PhysicalPageAddress ) {
	return WhSiGetNextPhysicalPages( Partition, 1, PhysicalPageAddress );
}

/**
 * @brief Internal helper to allocate host memory to guest physical memory
 *
 * @param Partition The VM partition
 * @param HostVa
 * @param GuestPa
 * @param Size
 * @param Flags
 * @return A result code
 */
HRESULT WhSiAllocateGuestPhysicalMemory( WHSE_PARTITION* Partition, uintptr_t* HostVa, uintptr_t* GuestPa, size_t Size, WHSE_MEMORY_ACCESS_FLAGS Flags ) {
	uintptr_t physicalAddress = 0;
	auto hresult = WhSiGetNextPhysicalPage( Partition, &physicalAddress );
	if ( FAILED( hresult ) )
		return hresult;

	uintptr_t hostVa = 0;
	hresult = WhSeAllocateGuestPhysicalMemory( Partition, &hostVa, physicalAddress, Size, Flags );
	if ( FAILED( hresult ) )
		return hresult;

	*HostVa = hostVa;
	*GuestPa = physicalAddress;

	return S_OK;
}

/**
 * @brief Internal function to setup paging
 *
 * @param Partition The VM partition
 * @param Pml4PhysicalAddress
 * @return A result code
 */
HRESULT WhSiSetupPaging( WHSE_PARTITION* Partition, uintptr_t* Pml4PhysicalAddress ) {
	// Check if already initialized
	//
	if ( Partition->MemoryLayout.Pml4HostVa != 0 )
		return HRESULT_FROM_WIN32( ERROR_ALREADY_INITIALIZED );

	// Allocate PML4 on physical memory
	//
	uintptr_t pml4Gpa = 0;
	uintptr_t pml4Hva = 0;
	auto hresult = WhSiAllocateGuestPhysicalMemory( Partition, &pml4Hva, &pml4Gpa, PAGE_SIZE, WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite );
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
	
	return hresult;
}

/**
 * @brief Internal function to insert page table in the paging directory
 * 
 * Internal function to insert page table in the paging directory
 * Allocate PML4 entry, PDP entry, PD entry and PT entry
 *
 * @param Partition The VM partition
 * @param VirtualAddress
 * @return A result code
 */
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
		return HRESULT_FROM_WIN32( ERROR_INTERNAL_ERROR );
	}
	
	// Search entry in Page Directory Pointers
	// 
	uintptr_t pdpHva = 0;
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

	// Search entry in Page Directories
	//
	uintptr_t pdHva = 0;
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

	// Add entry in Page Tables
	//
	uintptr_t ptHva = 0;
	hresult = WhSpLookupHVAFromPFN( Partition, pde.PageFrameNumber, &ptHva );
	if ( FAILED( hresult ) )
		return hresult;

	auto pt = reinterpret_cast< PMMPTE_HARDWARE >( ptHva );
	auto ppte = &pt[ ptIdx ];
	if ( ppte->Valid == FALSE ) {
		// Get the next available physical page
		//
		uintptr_t gpa { };
		auto hresult = WhSiGetNextPhysicalPage( Partition, &gpa );
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

		*ppte = pte;
	}

	return S_OK;
}

/**
 * @brief Find a suitable Guest VA
 *
 * @param Partition The VM partition
 * @param GuestVa
 * @param Size
 * @return A result code
 */
HRESULT WhSiFindBestGVA( WHSE_PARTITION* Partition, uintptr_t* GuestVa, size_t Size ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( GuestVa == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto tracker = Partition->MemoryLayout.AllocationTracker;
	if ( tracker == nullptr )
		return HRESULT_FROM_WIN32( PEERDIST_ERROR_NOT_INITIALIZED );

	auto first = reinterpret_cast< WHSE_ALLOCATION_NODE* >( GetDListHead( tracker ) );
	if ( first == nullptr )
		return HRESULT_FROM_WIN32( ERROR_NO_MORE_ITEMS );

	uintptr_t highestExistingGva = 0;
	size_t highestExistingGvaSize = 0;
	auto current = first;
	while ( current != nullptr ) {
		uintptr_t currentGva = current->GuestVirtualAddress;
		if ( currentGva > highestExistingGva ) {
			highestExistingGva = currentGva;
		}

		current = reinterpret_cast< WHSE_ALLOCATION_NODE* >( current->Next );
	}

	if ( current == nullptr )
		return HRESULT_FROM_WIN32( ERROR_NO_MORE_ITEMS );

	auto va = ALIGN_UP( highestExistingGva + highestExistingGvaSize );

	*GuestVa = va;

	return S_OK;
}

constexpr uint8_t MAKE_IDT_ATTRS( uint8_t Dpl, uint8_t GateType ) {
	return (
		( 1 << 7 )					// Present bit
		| ( ( Dpl << 6 ) & 0b11 )
		| ( 0 << 4 )				// Reserved bit
		| ( GateType & 0b1111 )
	);
}

/**
 * @brief Setup GDT
 *
 * @param Partition The VM partition
 * @param Registers
 * @return A result code
 */
HRESULT WhSiSetupGlobalDescriptorTable( WHSE_PARTITION* Partition, WHSE_REGISTERS Registers ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( Registers == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	// Allocate GDT
	//
	uintptr_t gdtHva = 0;
	uintptr_t gdtGva = 0xffff8000'00000000;
	auto hresult = WhSeAllocateGuestVirtualMemory( Partition, &gdtHva, gdtGva, PAGE_SIZE, WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite );
	if ( FAILED( hresult ) )
		return hresult;

	// Create the descriptors
	//
	uintptr_t base = 0;
	ptrdiff_t limit = 0xfffff;

	GDT_ENTRY nullDesc { 0 };
	hresult = WhSpCreateGdtEntry( &nullDesc, 0, 0, 0x00, 0x0 );
	if ( FAILED( hresult ) )
		return hresult;

	GDT_ENTRY kernelModeCodeSegmentDesc { 0 };
	hresult = WhSpCreateGdtEntry( &kernelModeCodeSegmentDesc, base, limit, 0x9a, 0xa );
	if ( FAILED( hresult ) )
		return hresult;

	GDT_ENTRY kernelModeDataSegmentDesc { 0 };
	hresult = WhSpCreateGdtEntry( &kernelModeDataSegmentDesc, base, limit, 0x92, 0xc );
	if ( FAILED( hresult ) )
		return hresult;

	GDT_ENTRY userModeCodeSegmentDesc { 0 };
	hresult = WhSpCreateGdtEntry( &userModeCodeSegmentDesc, base, limit, 0xfa, 0xa );
	if ( FAILED( hresult ) )
		return hresult;

	GDT_ENTRY userModeDataSegmentDesc { 0 };
	hresult = WhSpCreateGdtEntry( &userModeDataSegmentDesc, base, limit, 0xf2, 0xc );
	if ( FAILED( hresult ) )
		return hresult;

	// Allocate a page for the TSS
	//
	uintptr_t tssHva = 0;
	uintptr_t tssGva = 0xffffa000'00000000;
	hresult = WhSeAllocateGuestVirtualMemory( Partition, &tssHva, tssGva, sizeof( X64_TASK_STATE_SEGMENT ), WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite );
	if ( FAILED( hresult ) )
		return hresult;

	hresult = WhSpInitializeTss( Partition, reinterpret_cast< PX64_TASK_STATE_SEGMENT >( tssHva ) );
	if ( FAILED( hresult ) )
		return hresult;

	X64_TSS_ENTRY tssSegmentDesc { 0 };
	hresult = WhSpCreateTssEntry( &tssSegmentDesc, tssGva, sizeof( X64_TSS_ENTRY ), 0x89, 0x00 );
	if ( FAILED( hresult ) )
		return hresult;

	// Load the temp descriptors in memory
	//
	auto gdt = reinterpret_cast< PGDT_ENTRY >( gdtHva );
	
	// Offset : 0x0000	Use : Null Descriptor
	//
	gdt[ 0 ] = nullDesc;

	// Offset : 0x0008	Use : Kernel Mode Code Segment
	//
	gdt[ 1 ] = kernelModeCodeSegmentDesc;

	// Offset : 0x0010	Use : Kernel Mode Data Segment
	//
	gdt[ 2 ] = kernelModeDataSegmentDesc;

	// Offset : 0x0018	Use : User Mode Code Segment
	//
	gdt[ 3 ] = userModeCodeSegmentDesc;

	// Offset : 0x0020	Use : User Mode Data Segment
	//
	gdt[ 4 ] = userModeDataSegmentDesc;

	// Offset : 0x0028	Use : 64-bit Task State Segment
	//
	*( PX64_TSS_ENTRY ) ( &( gdt[ 5 ] ) ) = tssSegmentDesc;

	// Load GDTR
	//
	Registers[ Gdtr ].Table.Base = gdtGva;
	Registers[ Gdtr ].Table.Limit = static_cast< uint16_t >( sizeof( X64_TSS_ENTRY ) + ( sizeof( GDT_ENTRY ) * NUMBER_OF_GDT_DESCRIPTORS - 1 ) );

	// Load TR
	//
	Registers[ Tr ].Segment.Selector = 0x0028;

	return S_OK;
}

/**
 * @brief Setup IDT
 *
 * @param Partition The VM partition
 * @param Registers
 * @return A result code
 */
HRESULT WhSiSetupInterruptDescriptorTable( WHSE_PARTITION* Partition, WHSE_REGISTERS Registers ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( Registers == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	// Allocate two pages, one for the IDT
	//
	uintptr_t idtHva = 0;
	uintptr_t idtGva = 0xffff8000'00001000;
	auto hresult = WhSeAllocateGuestVirtualMemory( Partition, &idtHva, idtGva, PAGE_SIZE, WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite );
	if ( FAILED( hresult ) )
		return hresult;

	// The other one to trap ISR access
	//
	uintptr_t idtTrapHva = 0;
	uintptr_t idtTrapGva = 0xffff8000'00002000;
	hresult = WhSeAllocateGuestVirtualMemory( Partition, &idtTrapHva, idtTrapGva, PAGE_SIZE, WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite );
	if ( FAILED( hresult ) )
		return hresult;

	// Unmap the 2nd page immediately, keeping paging information but releasing the backing memory,
	// thus it will generate a memory access exception when trying to jump to the Interrupt
	// Service Routine
	//
	hresult = WhSeFreeGuestVirtualMemory( Partition, idtTrapHva, idtTrapGva, PAGE_SIZE );
	if ( FAILED( hresult ) )
		return hresult;

	// Fill IDT
	//
	auto ptr = idtTrapGva;
	auto idt = reinterpret_cast< PIDT_ENTRY >( idtHva );
	for ( auto idx = 0; idx < NUMBER_OF_IDT_DESCRIPTORS; idx++ ) {
		auto entry = IDT_ENTRY {
				.Low = static_cast< uint16_t >( ptr & UINT16_MAX ),
				.Selector = 0x0008, // Kernel CS index
				.InterruptStackTable = 0,
				.Attributes = MAKE_IDT_ATTRS( 0, 0b1110 ), // Make them traps
				.Mid = static_cast< uint16_t >( ( ptr >> 16 ) & UINT16_MAX ),
				.High = static_cast< uint32_t >( ( ptr >> 32 ) & UINT32_MAX ),
				.Reserved = 0
		};

		idt[ idx ] = entry;
		ptr += sizeof( decltype( ptr ) );
	}

	// Load IDTR
	//
	Registers[ Idtr ].Table.Base = idtGva;
	Registers[ Idtr ].Table.Limit = static_cast< uint16_t >( sizeof( IDT_ENTRY ) * NUMBER_OF_IDT_DESCRIPTORS - 1 );

	Partition->MemoryLayout.InterruptDescriptorTableVirtualAddress = idtTrapGva;

	return S_OK;
}