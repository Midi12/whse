#include "WinHvProcessor.hpp"
#include "WinHvUtils.hpp"

#include <memory.h>

/**
 * @brief Create a virtual processor in a partition
 *
 * @param Partition The VM partition
 * @param Mode The processor mode
 * @return A result code
 */
HRESULT WhSeCreateProcessor( WHSE_PARTITION* Partition, WHSE_PROCESSOR_MODE Mode ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	auto vp = &Partition->VirtualProcessor;
	vp->Index = 0;

	auto capabilities = WHV_CAPABILITY { 0 };
	uint32_t written = 0;

	auto hresult = ::WHvGetCapability( WHvCapabilityCodeProcessorVendor, &capabilities, sizeof( decltype( capabilities ) ), &written );
	if ( FAILED( hresult ) ) {
		return hresult;
	}

	vp->Vendor = capabilities.ProcessorVendor;

	hresult = ::WHvCreateVirtualProcessor( Partition->Handle, vp->Index, 0 );
	if ( FAILED( hresult ) )
		return hresult;

	auto registers = Partition->VirtualProcessor.Registers;
	hresult = WhSeGetProcessorRegisters( Partition, registers );
	if ( FAILED( hresult ) )
		return hresult;

	vp->Mode = Mode;

	uintptr_t lowestAddress = 0;
	uintptr_t highestAddress = 0;

	int ring;
	int codeSelector;
	int dataSelector;

	switch ( Mode ) {
		using enum WHSE_PROCESSOR_MODE;
	case UserMode:
		lowestAddress = 0x00000000'00000000;
		highestAddress = 0x00008000'00000000 - 64KiB;
		ring = 3;
		//codeSelector = 0x30;
		//dataSelector = 0x28;
		codeSelector = 0x18;
		dataSelector = 0x20;
		break;
	case KernelMode:
		lowestAddress = 0xffff8000'00000000;
		highestAddress = 0xffffffff'ffffffff - 4MiB;
		ring = 0;
		//codeSelector = 0x10;
		//dataSelector = 0x18;
		codeSelector = 0x08;
		dataSelector = 0x10;
		break;
	default:
		return HRESULT_FROM_WIN32( ERROR_NOT_SUPPORTED );
	}

	// Setup Virtual Address Space boundaries
	//
	Partition->MemoryLayout.VirtualAddressSpace.LowestAddress = lowestAddress;
	Partition->MemoryLayout.VirtualAddressSpace.HighestAddress = highestAddress;
	Partition->MemoryLayout.VirtualAddressSpace.SizeInBytes = ( highestAddress - lowestAddress ) - 1;

	// Setup segment registers
	//
	registers[ Cs ].Segment.Selector = ( codeSelector | ring );
	registers[ Cs ].Segment.DescriptorPrivilegeLevel = ring;
	registers[ Cs ].Segment.Long = 1;

	registers[ Ss ].Segment.Selector = ( dataSelector | ring );
	registers[ Ss ].Segment.DescriptorPrivilegeLevel = ring;
	registers[ Ss ].Segment.Default = 1;
	registers[ Ss ].Segment.Granularity = 1;

	registers[ Ds ] = registers[ Ss ];
	registers[ Es ] = registers[ Ss ];
	registers[ Gs ] = registers[ Ss ];

	// Set IF bit
	//
	registers[ Rflags ].Reg64 = MAKE_RFLAGS( 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );

	hresult = WhSeSetProcessorRegisters( Partition, registers );
	if ( FAILED( hresult ) )
		return hresult;

	return hresult;
}

/**
 * @brief Set a virtual processor registers
 *
 * @param Partition The VM partition
 * @param Registers The registers array to set the values 
 * @return A result code
 */
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

/**
 * @brief Get a virtual processor registers
 *
 * @param Partition The VM partition
 * @param Registers The registers array receiving the values 
 * @return A result code
 */
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

/**
 * @brief Run a virtual processor
 *
 * @param Partition The VM partition
 * @param ExitReason The returned exit reason
 * @return A result code
 */
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

	hresult = WhSeGetProcessorRegisters( Partition, vp->Registers );
	if ( FAILED( hresult ) )
		return hresult;

	bool retry = false;
	auto exitCallbacks = Partition->ExitCallbacks.Callbacks;
	auto isrCallbacks = Partition->IsrCallbacks.Callbacks;

	switch ( vp->ExitContext.ExitReason ) {
		using enum WHSE_EXIT_CALLBACK_SLOT;
		using enum WHV_RUN_VP_EXIT_REASON;

		case WHvRunVpExitReasonCanceled:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_USER_CANCELED_CALLBACK >( exitCallbacks[ UserCanceled ] );
				if ( callback != nullptr )
					retry = callback( Partition, &vp->ExitContext.VpContext, &vp->ExitContext.CancelReason );
				else
					return HRESULT_FROM_WIN32( ERROR_INTERNAL_ERROR );
			}
			break;
		case WHvRunVpExitReasonMemoryAccess:
			{
				// Check if the guest virtual address matches the trap page
				//
				if ( ( vp->ExitContext.MemoryAccess.Gva & 0xffffffff'fffff000 ) == Partition->MemoryLayout.InterruptDescriptorTableVirtualAddress ) {
					// Handle through special callbacks
					//
					auto offset = vp->ExitContext.MemoryAccess.Gva & 0x00000000'00000fff;

					auto index = offset / sizeof( uintptr_t );

					auto callback = Partition->IsrCallbacks.Callbacks[ static_cast< uint8_t >( index ) ];
					if ( callback != nullptr )
						retry = callback( Partition );
					else
						return HRESULT_FROM_WIN32( ERROR_INTERNAL_ERROR );
				}
				else {
					// Handle normally
					//
					auto callback = reinterpret_cast< WHSE_EXIT_MEMORYACCESS_CALLBACK >( exitCallbacks[ MemoryAccess ] );
					if ( callback != nullptr )
						retry = callback( Partition, &vp->ExitContext.VpContext, &vp->ExitContext.MemoryAccess );
				}
			}
			break;
		case WHvRunVpExitReasonX64IoPortAccess:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_IO_PORT_ACCESS_CALLBACK >( exitCallbacks[ IoPortAccess ] );
				if ( callback != nullptr )
					retry = callback( Partition, &vp->ExitContext.VpContext, &vp->ExitContext.IoPortAccess );
				else
					return HRESULT_FROM_WIN32( ERROR_INTERNAL_ERROR );
			}
			break;
		case WHvRunVpExitReasonUnrecoverableException:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_UNRECOVERABLE_EXCEPTION_CALLBACK >( exitCallbacks[ UnrecoverableException ] );
				if ( callback != nullptr )
					retry = callback( Partition, &vp->ExitContext.VpContext, nullptr );
				else
					return HRESULT_FROM_WIN32( ERROR_INTERNAL_ERROR );
			}
			break;
		case WHvRunVpExitReasonInvalidVpRegisterValue:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_INVALID_REGISTER_VALUE_CALLBACK >( exitCallbacks[ InvalidVpRegisterValue ] );
				if ( callback != nullptr )
					retry = callback( Partition, &vp->ExitContext.VpContext, nullptr );
				else
					return HRESULT_FROM_WIN32( ERROR_INTERNAL_ERROR );
			}
			break;
		case WHvRunVpExitReasonUnsupportedFeature:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_UNSUPPORTED_FEATURE_CALLBACK >( exitCallbacks[ UnsupportedFeature ] );
				if ( callback != nullptr )
					retry = callback( Partition, &vp->ExitContext.VpContext, &vp->ExitContext.UnsupportedFeature );
				else
					return HRESULT_FROM_WIN32( ERROR_INTERNAL_ERROR );
			}
			break;
		case WHvRunVpExitReasonX64InterruptWindow:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_INTERRUPTION_DELIVERY_CALLBACK >( exitCallbacks[ InterruptWindow ] );
				if ( callback != nullptr )
					retry = callback( Partition, &vp->ExitContext.VpContext, &vp->ExitContext.InterruptWindow );
				else
					return HRESULT_FROM_WIN32( ERROR_INTERNAL_ERROR );
			}
			break;
		case WHvRunVpExitReasonX64Halt:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_HALT_CALLBACK >( exitCallbacks[ Halt ] );
				if ( callback != nullptr )
					retry = callback( Partition, &vp->ExitContext.VpContext, nullptr );
				else
					return HRESULT_FROM_WIN32( ERROR_INTERNAL_ERROR );
			}
			break;
		case WHvRunVpExitReasonX64ApicEoi:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_APIC_EOI_CALLBACK >( exitCallbacks[ ApicEoi ] );
				if ( callback != nullptr )
					retry = callback( Partition, &vp->ExitContext.VpContext, &vp->ExitContext.ApicEoi );
				else
					return HRESULT_FROM_WIN32( ERROR_INTERNAL_ERROR );
			}
			break;
		case WHvRunVpExitReasonX64MsrAccess:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_MSR_ACCESS_CALLBACK >( exitCallbacks[ MsrAccess ] );
				if ( callback != nullptr )
					retry = callback( Partition, &vp->ExitContext.VpContext, &vp->ExitContext.MsrAccess );
				else
					return HRESULT_FROM_WIN32( ERROR_INTERNAL_ERROR );
			}
			break;
		case WHvRunVpExitReasonX64Cpuid:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_CPUID_ACCESS_CALLBACK >( exitCallbacks[ Cpuid ] );
				if ( callback != nullptr )
					retry = callback( Partition, &vp->ExitContext.VpContext, &vp->ExitContext.CpuidAccess );
				else
					return HRESULT_FROM_WIN32( ERROR_INTERNAL_ERROR );
			}
			break;
		case WHvRunVpExitReasonException:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_VP_EXCEPTION_CALLBACK >( exitCallbacks[ Exception ] );
				if ( callback != nullptr )
					retry = callback( Partition, &vp->ExitContext.VpContext, &vp->ExitContext.VpException );
				else
					return HRESULT_FROM_WIN32( ERROR_INTERNAL_ERROR );
			}
			break;
		case WHvRunVpExitReasonX64Rdtsc:
			{
				auto callback = reinterpret_cast< WHSE_EXIT_RDTSC_ACCESS_CALLBACK >( exitCallbacks[ Rdtsc ] );
				if ( callback != nullptr )
					retry = callback( Partition, &vp->ExitContext.VpContext, &vp->ExitContext.ReadTsc );
				else
					return HRESULT_FROM_WIN32( ERROR_INTERNAL_ERROR );
			}
			break;
		case WHvRunVpExitReasonNone:
		default:
			break;
	}

	if ( retry )
		goto retry;

	*ExitReason = vp->ExitContext.ExitReason;

	return hresult;
}

/**
 * @brief Delete a virtual processor from the partition
 *
 * @param Partition The VM partition
 * @return A result code
 */
HRESULT WhSeDeleteProcessor( WHSE_PARTITION* Partition ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	return ::WHvDeleteVirtualProcessor( Partition->Handle, Partition->VirtualProcessor.Index );
}