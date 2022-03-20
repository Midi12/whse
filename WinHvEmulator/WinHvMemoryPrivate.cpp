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