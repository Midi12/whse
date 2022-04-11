#include "WinHvPartition.hpp"
#include "WinHvAllocationTracker.hpp"

/**
 * @brief Create an hypervisor partition
 *
 * @param Partition The VM partition
 * @return A result code
 */
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
	*Partition = reinterpret_cast< WHSE_PARTITION* >( malloc( sizeof( decltype( **Partition ) ) ) );

	if ( *Partition == nullptr ) {
		// Allocation failed
		//
		hresult = HRESULT_FROM_WIN32( ERROR_OUTOFMEMORY );

		if ( partitionHandle ) {
			::WHvDeletePartition( partitionHandle );
		}

		return hresult;
	}
	else {
		// Set the handle
		//
		( *Partition )->Handle = partitionHandle;

		// Initialize partition
		//
		// Todo : Create WhSiInitializePartition
		hresult = WhSeInitializeAllocationTracker( *Partition );
	}

	return hresult;
}

/**
 * @brief Delete an hypervisor partition
 *
 * @param Partition The VM partition
 * @return A result code
 */
HRESULT WhSeDeletePartition( WHSE_PARTITION** Partition ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( *Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	// Set SUCCESS as default, in case we don't have any sub-object(s) to free
	//
	HRESULT hresult = 0;

	auto partition = *Partition;

	// Cleanup and free the partition
	//
	if ( partition->Handle ) {
		hresult = WhSeFreeAllocationTracker( partition );
		if ( FAILED( hresult ) )
			return hresult;

		hresult = ::WHvDeletePartition( partition->Handle );
		if ( FAILED( hresult ) )
			return hresult;

		partition->Handle = nullptr;
	}

	free( partition );
	*Partition = nullptr;

	return hresult;
}