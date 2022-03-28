#include "WinHvMemoryPrivate.hpp"
#include "WinHvUtils.hpp"
#include "WinHvAllocationTracker.hpp"

/**
 * @brief Private api
 *
 * @param Partition The VM partition
 * @param ParentLayer
 * @param Index
 * @return A result code
 */
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

	return hresult;
}

/**
 * @brief Private api
 *
 * @param Partition The VM partition
 * @param PageFrameNumber
 * @param HostVa
 * @return A result code
 */
HRESULT WhSpLookupHVAFromPFN( WHSE_PARTITION* Partition, uintptr_t PageFrameNumber, PVOID* HostVa ) {
	WHSE_ALLOCATION_NODE* node = nullptr;
	auto hresult = WhSeFindAllocationNodeByGpa( Partition, PageFrameNumber * PAGE_SIZE, &node );
	if ( FAILED( hresult ) )
		return hresult;

	*HostVa = node->HostVirtualAddress;

	return hresult;
}

/**
 * @brief Private api
 *
 * @param Entry The returned GDT entry
 * @param Base
 * @param Limit
 * @param Access
 * @param Flags
 * @return A result code
 */
HRESULT WhSpCreateGdtEntry( GDT_ENTRY* Entry, uintptr_t Base, ptrdiff_t Limit, uint8_t Access, uint8_t Flags ) {
	if ( Entry == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	Entry->LimitLow = static_cast< uint16_t >( Limit & 0xffff );
	Entry->BaseLow = static_cast< uint16_t >( Base & 0xffff );
	Entry->BaseMid = static_cast< uint8_t >( ( Base >> 16 ) & 0xff );
	Entry->Access = Access;
	Entry->LimitHigh = static_cast< uint8_t >( ( Limit >> 16 ) & 0xf );
	Entry->Flags = static_cast< uint8_t >( Flags & 0xf );
	Entry->BaseHigh = static_cast< uint8_t >( ( Base >> 24 ) & 0xff );

	return S_OK;
}