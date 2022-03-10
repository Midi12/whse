#include "WinHvProcessor.hpp"

#include <memory.h>

// Create a virtual processor in a partition
//
HRESULT WhSeCreateProcessor( WHSE_PARTITION* Partition ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto vp = &Partition->VirtualProcessor;
	vp->Index = 0;

	auto hresult = ::WHvCreateVirtualProcessor( Partition->Handle, vp->Index, 0 );
	if ( FAILED( hresult ) )
		return hresult;

	auto registers = Partition->VirtualProcessor.Registers;
	return WhSeGetProcessorRegisters( Partition, registers );
}

// Set a virtual processor registers
//
HRESULT WhSeSetProcessorRegisters( WHSE_PARTITION* Partition, WHSE_REGISTERS Registers ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( Registers == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto vp = &Partition->VirtualProcessor;
	if ( vp->Registers != Registers )
		memcpy_s( vp->Registers, sizeof( decltype( *vp->Registers ) ), Registers, sizeof( decltype( *vp->Registers ) ) );

	return ::WHvSetVirtualProcessorRegisters( Partition->Handle, vp->Index, g_registers, g_registers_count, vp->Registers );
}

// Get a virtual processor registers
//
HRESULT WhSeGetProcessorRegisters( WHSE_PARTITION* Partition, WHSE_REGISTERS Registers ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( Registers == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto vp = &Partition->VirtualProcessor;

	auto hresult = ::WHvGetVirtualProcessorRegisters( Partition->Handle, vp->Index, g_registers, g_registers_count, vp->Registers );
	if ( FAILED( hresult ) )
		return hresult;

	if ( vp->Registers != Registers )
		Registers = vp->Registers;

	return hresult;
}

// Run a virtual processor registers
//
HRESULT WhSeRunProcessor( WHSE_PARTITION* Partition, WHSE_VP_EXIT_REASON* ExitReason ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( ExitReason == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );
	
	auto vp = &Partition->VirtualProcessor;

	auto hresult = ::WHvRunVirtualProcessor( Partition->Handle, vp->Index, &vp->ExitContext, sizeof( decltype( vp->ExitContext ) ) );
	if ( FAILED( hresult ) )
		return hresult;

	*ExitReason = vp->ExitContext.ExitReason;

	return WhSeGetProcessorRegisters( Partition, vp->Registers );
}

// Delete a virtual processor from the partition
//
HRESULT WhSeDeleteProcessor( WHSE_PARTITION* Partition ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	return ::WHvDeleteVirtualProcessor( Partition->Handle, Partition->VirtualProcessor.Index );
}