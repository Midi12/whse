#ifndef WINHVMEMORYPRIVATE_HPP
#define WINHVMEMORYPRIVATE_HPP

#include <windows.h>

#include "WinHvPartition.hpp"
#include "WinHvMemoryInternal.hpp"

// Private api
//
HRESULT WhSpLookupHVAFromPFN( WHSE_PARTITION* Partition, uintptr_t PageFrameNumber, uintptr_t* HostVa );

// Private api
//
HRESULT WhSpInsertPageTableEntry( WHSE_PARTITION* Partition, PMMPTE_HARDWARE ParentLayer, uint16_t Index );

// Private api
//
HRESULT WhSpCreateGdtEntry( GDT_ENTRY* Entry, uintptr_t Base, ptrdiff_t Limit, uint8_t Access, uint8_t Flags );

// Private api
//
HRESULT WhSpCreateTssEntry( X64_TSS_ENTRY* TssSegmentDesc, uintptr_t Base, ptrdiff_t Limit, uint8_t Access, uint8_t Flags );

// Private api
//
HRESULT WhSpInitializeTss( WHSE_PARTITION* Partition, PX64_TASK_STATE_SEGMENT Tss );

#endif // !WINHVMEMORYPRIVATE_HPP