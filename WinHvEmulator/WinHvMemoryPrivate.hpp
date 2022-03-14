#ifndef WINHVMEMORYPRIVATE_HPP
#define WINHVMEMORYPRIVATE_HPP

#include <windows.h>

#include "WinHvPartition.hpp"
#include "WinHvMemoryInternal.hpp"

// Private api
//
HRESULT WhSpLookupHVAFromPFN( WHSE_PARTITION* Partition, uintptr_t PageFrameNumber, PVOID* HostVa );

// Private api
//
HRESULT WhSpInsertPageTableEntry( WHSE_PARTITION* Partition, PMMPTE_HARDWARE ParentLayer, uint16_t Index );

#endif // !WINHVMEMORYPRIVATE_HPP