#ifndef WINHVMEMORYALLOCATIONTRACKER_HPP
#define WINHVMEMORYALLOCATIONTRACKER_HPP

#include <windows.h>

#include "WinHvExports.hpp"
#include "WinHvPartition.hpp"

// Initialize allocation tracker structures
//
HRESULT WHSEAPI WhSeInitializeAllocationTracker( WHSE_PARTITION* Partition );

// Free allocation tracker structures
//
HRESULT WHSEAPI WhSeFreeAllocationTracker( WHSE_PARTITION* Partition );

typedef bool ( *WHSE_ALLOCATION_TRACKER_PREDICATE )( const WHSE_ALLOCATION_NODE* );

// Find the first allocation node matching the predicate
//
HRESULT WHSEAPI WhSeFindAllocationNode( WHSE_PARTITION* Partition, WHSE_ALLOCATION_TRACKER_PREDICATE Predicate, WHSE_ALLOCATION_NODE** Node );

// Find the first allocation node matching the guest virtual address
//
HRESULT WHSEAPI WhSeFindAllocationNodeByGva( WHSE_PARTITION* Partition, uintptr_t GuestVa, WHSE_ALLOCATION_NODE** Node );

// Find the first allocation node matching the guest physical address
//
HRESULT WHSEAPI WhSeFindAllocationNodeByGpa( WHSE_PARTITION* Partition, uintptr_t GuestPa, WHSE_ALLOCATION_NODE** Node );

// Insert a tracking node
//
HRESULT WHSEAPI WhSeInsertAllocationTrackingNode( WHSE_PARTITION* Partition, WHSE_ALLOCATION_NODE Node );

#endif // !WINHVMEMORYALLOCATIONTRACKER_HPP