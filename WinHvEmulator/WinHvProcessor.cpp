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

	retry:
	auto hresult = ::WHvRunVirtualProcessor( Partition->Handle, vp->Index, &vp->ExitContext, sizeof( decltype( vp->ExitContext ) ) );
	if ( FAILED( hresult ) )
		return hresult;

	bool retry = false;
	auto callbacks = reinterpret_cast< PVOID >( Partition->ExitCallbacks );

	switch ( vp->ExitContext.ExitReason ) {
		using enum WHSE_EXIT_CALLBACK_SLOT;

		case WHvRunVpExitReasonCanceled:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_USER_CANCELED_CALLBACK >( callbacks[ UserCanceled ] );
				retry = callback( Partition, vp->ExitContext.VpContext, vp->ExitContext.CancelReason );
			}
			break;
		case WHvRunVpExitReasonMemoryAccess:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_MEMORYACCESS_CALLBACK >( callbacks[ MemoryAccess ] );
				retry = callback( Partition, vp->ExitContext.VpContext, vp->ExitContext.MemoryAccess );
			}
			break;
		case WHvRunVpExitReasonX64IoPortAccess:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_IO_PORT_ACCESS_CALLBACK >( callbacks[ IoPortAccess ] );
				retry = callback( Partition, vp->ExitContext.VpContext, vp->ExitContext.IoPortAccess );
			}
			break;
		case WHvRunVpExitReasonUnrecoverableException:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_UNRECOVERABLE_EXCEPTION_CALLBACK >( callbacks[ UnrecoverableException ] );
				retry = callback( Partition, vp->ExitContext.VpContext, nullptr );
			}
			break;
		case WHvRunVpExitReasonInvalidVpRegisterValue:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_INVALID_REGISTER_VALUE_CALLBACK >( callbacks[ InvalidVpRegisterValue ] );
				retry = callback( Partition, vp->ExitContext.VpContext, nullptr );
			}
			break;
		case WHvRunVpExitReasonUnsupportedFeature:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_UNSUPPORTED_FEATURE_CALLBACK >( callbacks[ UnsupportedFeature ] );
				retry = callback( Partition, vp->ExitContext.VpContext, vp->ExitContext.UnsupportedFeature );
			}
			break;
		case WHvRunVpExitReasonX64InterruptWindow:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_INTERRUPTION_DELIVERY_CALLBACK >( callbacks[ InterruptWindow ] );
				retry = callback( Partition, vp->ExitContext.VpContext, vp->ExitContext.InterruptWindow );
			}
			break;
		case WHvRunVpExitReasonX64Halt:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_HALT_CALLBACK >( callbacks[ Halt ] );
				retry = callback( Partition, vp->ExitContext.VpContext, nullptr );
			}
			break;
		case WHvRunVpExitReasonX64ApicEoi:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_APIC_EOI_CALLBACK >( callbacks[ ApicEoi ] );
				retry = callback( Partition, vp->ExitContext.VpContext, vp->ExitContext.ApicEoi );
			}
			break;
		case WHvRunVpExitReasonX64MsrAccess:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_MSR_ACCESS_CALLBACK >( callbacks[ MsrAccess ] );
				retry = callback( Partition, vp->ExitContext.VpContext, vp->ExitContext.MsrAccess );
			}
			break;
		case WHvRunVpExitReasonX64Cpuid:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_CPUID_ACCESS_CALLBACK >( callbacks[ Cpuid ] );
				retry = callback( Partition, vp->ExitContext.VpContext, vp->ExitContext.CpuidAccess );
			}
			break;
		case WHvRunVpExitReasonException:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_VP_EXCEPTION_CALLBACK >( callbacks[ Exception ] );
				retry = callback( Partition, vp->ExitContext.VpContext, vp->ExitContext.VpException );
			}
			break;
		case WHvRunVpExitReasonX64Rdtsc:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_RDTSC_ACCESS_CALLBACK >( callbacks[ Rdtsc ] );
				retry = callback( Partition, vp->ExitContext.VpContext, vp->ExitContext.ReadTsc );
			}
			break;
		case WHvRunVpExitReasonNone:
		default:
			break;
	}

	if ( retry )
		goto retry;

	*ExitReason = vp->ExitContext.ExitReason;

	return WhSeGetProcessorRegisters( Partition, vp->Register );
}

// Delete a virtual processor from the partition
//
HRESULT WhSeDeleteProcessor( WHSE_PARTITION* Partition ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	return ::WHvDeleteVirtualProcessor( Partition->Handle, Partition->VirtualProcessor.Index );
}