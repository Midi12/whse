#include "WinHvPartition.hpp"

// Create an hypervisor partition
//
HRESULT WhSeCreatePartition( WHSE_PARTITION** Partition ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( *Partition != nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	WHV_PARTITION_HANDLE partitionHandle { };

	// Create hypervisor partition
	//
	auto hresult = ::WHvCreatePartition( &partitionHandle );
	if ( FAILED( hresult ) )
		return hresult;

	// Allow a single processor
	//
	auto property = WHV_PARTITION_PROPERTY { 0 };
	property.ProcessorCount = 1;
	hresult = ::WHvSetPartitionProperty( partitionHandle, WHvPartitionPropertyCodeProcessorCount, &property, sizeof( decltype( property ) ) );
	if ( FAILED( hresult ) )
		return hresult;

	// Setup the partition
	//
	hresult = ::WHvSetupPartition( partitionHandle );
	if ( FAILED( hresult ) )
		return hresult;

	// Allocate the partition structure
	//
	*Partition = reinterpret_cast< WHSE_PARTITION* >( ::HeapAlloc( ::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( decltype( **Partition ) ) ) );

	if ( *Partition == nullptr ) {
		// Allocation failed
		//
		hresult = HRESULT_FROM_WIN32( ERROR_OUTOFMEMORY );

		if ( partitionHandle ) {
			::WHvDeletePartition( partitionHandle );
		}
	}
	else {
		// Set the handle
		//
		( *Partition )->Handle = partitionHandle;
	}

	return hresult;
}

// Delete an hypervisor partition
//
HRESULT WhSeDeletePartition( WHSE_PARTITION** Partition ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( *Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	// Set SUCCESS as default, in case we don't have any sub-object(s) to free
	//
	HRESULT hresult = 0;

	auto partition = *Partition;

	// Free the partition
	//
	if ( partition->Handle ) {
		hresult = ::WHvDeletePartition( partition->Handle );
		if ( FAILED( hresult ) ) {
			return hresult;
		}

		partition->Handle = nullptr;
	}

	::HeapFree( ::GetProcessHeap(), 0, partition );
	*Partition = nullptr;

	return hresult;
}