#include "DoubleLinkedList.hpp"
#include "WinHvAllocationTracker.hpp"
#include "WinHvMemory.hpp"


/**
 * @brief Initialize allocation tracker structures
 *
 * @param Partition The VM partition
 * @return A result code
 */
HRESULT WhSeInitializeAllocationTracker( WHSE_PARTITION* Partition ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( PEERDIST_ERROR_NOT_INITIALIZED );
	
	// Initialize address space allocations tracking
	//
	auto tracker = &( Partition->MemoryLayout.MemoryArena.AllocatedMemoryBlocks );

	InitializeDListHeader( tracker );

	return S_OK;
}

/**
 * @brief Free allocation tracker structures
 *
 * @param Partition The VM partition
 * @return A result code
 */
HRESULT WhSeFreeAllocationTracker( WHSE_PARTITION* Partition ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto tracker = &( Partition->MemoryLayout.MemoryArena.AllocatedMemoryBlocks );

	auto first = reinterpret_cast< WHSE_ALLOCATION_NODE* >( GetDListHead( tracker ) );
	if ( first == nullptr )
		return HRESULT_FROM_WIN32( ERROR_NO_MORE_ITEMS );

	// Release all allocation on backing host memory
	//
	auto current = first;
	while ( current != nullptr ) {
		if ( current->GuestPhysicalAddress != 0 && current->GuestVirtualAddress == 0 )
			WhSeFreeGuestPhysicalMemory( Partition, current->HostVirtualAddress, current->GuestPhysicalAddress, current->Size );
		else if ( current->GuestPhysicalAddress != 0 && current->GuestVirtualAddress != 0 )
			WhSeFreeGuestVirtualMemory( Partition, current->HostVirtualAddress, current->GuestVirtualAddress, current->Size );
		else
			DebugBreak();

		current = reinterpret_cast< WHSE_ALLOCATION_NODE* >( current->Next );
	}

	FlushDList( tracker );

	return S_OK;
}

/**
 * @brief Find the first allocation node matching the predicate
 *
 * @param Partition The VM partition
 * @param Predicate The predicate to match
 * @param Node The node to be returned
 * @return A result code
 */
HRESULT WhSeFindAllocationNode( WHSE_PARTITION* Partition, WHSE_ALLOCATION_TRACKER_PREDICATE Predicate, WHSE_ALLOCATION_NODE** Node ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( Node == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( *Node != nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto tracker = &( Partition->MemoryLayout.MemoryArena.AllocatedMemoryBlocks );

	auto first = reinterpret_cast< WHSE_ALLOCATION_NODE* >( GetDListHead( tracker ) );
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

	return *Node != nullptr ? S_OK : HRESULT_FROM_WIN32( ERROR_NOT_FOUND );
}

/**
 * @brief Find the first allocation node matching the guest virtual address
 *
 * @param Partition The VM partition
 * @param GuestVa The guest virtual address to match
 * @param Node The node to be returned
 * @return A result code
 */
HRESULT WhSeFindAllocationNodeByGva( WHSE_PARTITION* Partition, uintptr_t GuestVa, WHSE_ALLOCATION_NODE** Node ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( Node == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( *Node != nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto tracker = &( Partition->MemoryLayout.MemoryArena.AllocatedMemoryBlocks );

	auto first = reinterpret_cast< WHSE_ALLOCATION_NODE* >( GetDListHead( tracker ) );

	// Iterate over the list
	//
	auto current = first;
	while ( current != nullptr ) {
		if ( current->GuestVirtualAddress <= GuestVa && GuestVa < ( current->GuestVirtualAddress + current->Size ) ) {
			*Node = current;
			break;
		}

		current = reinterpret_cast< WHSE_ALLOCATION_NODE* >( current->Next );
	}

	return HRESULT_FROM_WIN32( *Node != nullptr ? S_OK : ERROR_NOT_FOUND );
}

/**
 * @brief Find the first allocation node matching the guest physical address
 *
 * @param Partition The VM partition
 * @param GuestPa The guest physical address to match
 * @param Node The node to be returned
 * @return A result code
 */
HRESULT WhSeFindAllocationNodeByGpa( WHSE_PARTITION* Partition, uintptr_t GuestPa, WHSE_ALLOCATION_NODE** Node ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( Node == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( *Node != nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto tracker = &( Partition->MemoryLayout.MemoryArena.AllocatedMemoryBlocks );

	auto first = reinterpret_cast< WHSE_ALLOCATION_NODE* >( GetDListHead( tracker ) );

	// Iterate over the list
	//
	auto current = first;
	while ( current != nullptr ) {
		if ( current->GuestPhysicalAddress <= GuestPa && GuestPa < ( current->GuestPhysicalAddress + current->Size ) ) {
			*Node = current;
			break;
		}

		current = reinterpret_cast< WHSE_ALLOCATION_NODE* >( current->Next );
	}

	return HRESULT_FROM_WIN32( *Node != nullptr ? S_OK : ERROR_NOT_FOUND );
}

/**
 * @brief Insert a tracking node
 *
 * @param Partition The VM partition
 * @param Node The node to insert
 * @return A result code
 */
HRESULT WHSEAPI WhSeInsertAllocationTrackingNode( WHSE_PARTITION* Partition, WHSE_ALLOCATION_NODE Node )
{
	WHSE_ALLOCATION_NODE* node = reinterpret_cast< WHSE_ALLOCATION_NODE* >( malloc( sizeof( decltype( *node ) ) ) );
	if ( node == nullptr )
		return HRESULT_FROM_WIN32( ERROR_OUTOFMEMORY );

	memcpy_s( node, sizeof( decltype( *node ) ), &Node, sizeof( decltype( Node ) ) );
	
	auto tracker = &( Partition->MemoryLayout.MemoryArena.AllocatedMemoryBlocks );
	::PushBackDListEntry( tracker, node );

	return S_OK;
}
