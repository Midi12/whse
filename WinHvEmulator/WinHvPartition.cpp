#include "WinHvPartition.hpp"

// Create an hypervisor partition
//
HRESULT WhSeCreatePartition( WHSE_PARTITION** pPartition ) {
	if ( pPartition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( *pPartition != nullptr )
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
	*pPartition = reinterpret_cast< WHSE_PARTITION* >( ::HeapAlloc( ::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( decltype( **pPartition ) ) ) );

	if ( *pPartition == nullptr ) {
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
		( *pPartition )->Handle = partitionHandle;
	}

	return hresult;
}

// Delete an hypervisor partition
//
HRESULT WhSeDeletePartition( WHSE_PARTITION* Partition ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	// Set SUCCESS as default, in case we don't have any sub-object(s) to free
	//
	HRESULT hresult = 0;

	// Free the partition
	if ( Partition->Handle ) {
		hresult = ::WHvDeletePartition( Partition->Handle );
		if ( FAILED( hresult ) ) {
			return hresult;
		}

		Partition->Handle = nullptr;
	}

	return hresult;
}