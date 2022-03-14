#include "WinHvAllocationTracker.hpp"
#include "WinHvMemory.hpp"

// Initialize allocation tracker structures
//
HRESULT WhSeInitializeAllocationTracker( WHSE_PARTITION* Partition ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( PEERDIST_ERROR_NOT_INITIALIZED );

	// Initialize address space allocations tracking
	//
	PSLIST_HEADER tracker = reinterpret_cast< PSLIST_HEADER >( ::HeapAlloc( ::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( decltype( *tracker ) ) ) );
	if ( tracker == nullptr )
		return HRESULT_FROM_WIN32( ERROR_OUTOFMEMORY );

	::InitializeSListHead( tracker );
	Partition->MemoryLayout.AllocationTracker = tracker;

	return S_OK;
}

// Free allocation tracker structures
//
HRESULT WhSeFreeAllocationTracker( WHSE_PARTITION* Partition ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto tracker = Partition->MemoryLayout.AllocationTracker;
	if ( tracker == nullptr )
		return HRESULT_FROM_WIN32( PEERDIST_ERROR_NOT_INITIALIZED );


	auto first = reinterpret_cast< WHSE_ALLOCATION_NODE* >( ::RtlFirstEntrySList( tracker ) );
	if ( first == nullptr )
		return HRESULT_FROM_WIN32( ERROR_NO_MORE_ITEMS );

	// Release all allocation on backing host memory
	//
	auto current = first;
	while ( current != nullptr ) {
		if ( current->GuestPhysicalAddress != 0 && current->GuestVirtualAddress == nullptr )
			WhSeFreeGuestPhysicalMemory( Partition, current->HostVirtualAddress, current->GuestPhysicalAddress, current->Size );
		else if ( current->GuestPhysicalAddress != 0 && current->GuestVirtualAddress != nullptr )
			WhSeFreeGuestVirtualMemory( Partition, current->HostVirtualAddress, reinterpret_cast< uintptr_t >( current->GuestVirtualAddress ), current->Size );
		else
			DebugBreak();

		current = reinterpret_cast< WHSE_ALLOCATION_NODE* >( current->Next );
	}

	::RtlInterlockedFlushSList( tracker );

	return S_OK;
}

// Find the first allocation node matching the predicate
//
HRESULT WhSeFindAllocationNode( WHSE_PARTITION* Partition, WHSE_ALLOCATION_TRACKER_PREDICATE Predicate, WHSE_ALLOCATION_NODE** Node ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( Node == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( *Node != nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto tracker = Partition->MemoryLayout.AllocationTracker;
	if ( tracker == nullptr )
		return HRESULT_FROM_WIN32( PEERDIST_ERROR_NOT_INITIALIZED );

	auto first = reinterpret_cast< WHSE_ALLOCATION_NODE* >( ::RtlFirstEntrySList( tracker ) );
	if ( first == nullptr )
		return HRESULT_FROM_WIN32( ERROR_NO_MORE_ITEMS );

	// Iterate over the list
	//
	auto current = first;
	while ( current != nullptr ) {
		if ( Predicate( current ) ) {
			*Node = current;
			break;
		}

		current = reinterpret_cast< WHSE_ALLOCATION_NODE* >( current->Next );
	}

	return HRESULT_FROM_WIN32( *Node != nullptr ? S_OK : ERROR_NOT_FOUND );
}

// Find the first allocation node matching the guest virtual address
//
HRESULT WhSeFindAllocationNodeByGva( WHSE_PARTITION* Partition, uintptr_t GuestVa, WHSE_ALLOCATION_NODE** Node ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( Node == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( *Node != nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto tracker = Partition->MemoryLayout.AllocationTracker;
	if ( tracker == nullptr )
		return HRESULT_FROM_WIN32( PEERDIST_ERROR_NOT_INITIALIZED );

	auto first = reinterpret_cast< WHSE_ALLOCATION_NODE* >( ::RtlFirstEntrySList( tracker ) );
	if ( first == nullptr )
		return HRESULT_FROM_WIN32( ERROR_NO_MORE_ITEMS );

	// Iterate over the list
	//
	auto current = first;
	while ( current != nullptr ) {
		if ( current->GuestVirtualAddress == reinterpret_cast< PVOID >( GuestVa ) ) {
			*Node = current;
			break;
		}

		current = reinterpret_cast< WHSE_ALLOCATION_NODE* >( current->Next );
	}

	return HRESULT_FROM_WIN32( *Node != nullptr ? S_OK : ERROR_NOT_FOUND );
}

// Find the first allocation node matching the guest physical address
//
HRESULT WhSeFindAllocationNodeByGpa( WHSE_PARTITION* Partition, uintptr_t GuestPa, WHSE_ALLOCATION_NODE** Node ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( Node == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( *Node != nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto tracker = Partition->MemoryLayout.AllocationTracker;
	if ( tracker == nullptr )
		return HRESULT_FROM_WIN32( PEERDIST_ERROR_NOT_INITIALIZED );

	auto first = reinterpret_cast< WHSE_ALLOCATION_NODE* >( ::RtlFirstEntrySList( tracker ) );
	if ( first == nullptr )
		return HRESULT_FROM_WIN32( ERROR_NO_MORE_ITEMS );

	// Iterate over the list
	//
	auto current = first;
	while ( current != nullptr ) {
		if ( current->GuestPhysicalAddress == GuestPa ) {
			*Node = current;
			break;
		}

		current = reinterpret_cast< WHSE_ALLOCATION_NODE* >( current->Next );
	}

	return HRESULT_FROM_WIN32( *Node != nullptr ? S_OK : ERROR_NOT_FOUND );
}

// Insert a tracking node
//
HRESULT WHSEAPI WhSeInsertAllocationTrackingNode( WHSE_PARTITION* Partition, WHSE_ALLOCATION_NODE Node )
{
	WHSE_ALLOCATION_NODE* node = reinterpret_cast< WHSE_ALLOCATION_NODE* >( ::HeapAlloc( ::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( decltype( *node ) ) ) );
	if ( node == nullptr )
		return HRESULT_FROM_WIN32( ERROR_OUTOFMEMORY );

	memcpy_s( node, sizeof( decltype( *node ) ), &Node, sizeof( decltype( Node ) ) );

	::InterlockedPushEntrySList( Partition->MemoryLayout.AllocationTracker, node );

	return S_OK;
}
