#include "Runner.hpp"
#include "Utils.hpp"

#include "winbase.h"
#include "winnt.h"

#include <windows.h>
#include <winhvplatform.h>

#include <stdio.h>
#include "../WinHvEmulator/WinHvAllocationTracker.hpp"

#define EXIT_WITH_MESSAGE( x ) \
	{ \
		printf( x "\n" ); \
		printf( "Last hresult = %llx\n", static_cast< unsigned long long >( WhSeGetLastHresult() ) ); \
		printf( "Last error code = %llx\n", static_cast< unsigned long long >( WhSeGetLastError() ) ); \
		return EXIT_FAILURE; \
	} \

constexpr size_t PAGE_SIZE = 4096;

constexpr size_t ALIGN_UP( size_t x ) {
	return ( ( PAGE_SIZE - 1 ) & x ) ? ( ( x + PAGE_SIZE ) & ~( PAGE_SIZE - 1 ) ) : x;
}

bool OnMemoryAccessExit( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_MEMORY_ACCESS_CONTEXT* ExitContext ) {
	printf( "HandleMemoryAccessExit" );

	// Fix the VA
	//
	PVOID hva = nullptr;
	size_t size = PAGE_SIZE;
	auto hresult = WhSeAllocateGuestVirtualMemory( Partition, &hva, ExitContext->Gva, &size, WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite );
	if ( FAILED( hresult ) )
		return false;

	// Ask to retry the current instruction
	//
	return true;
}

bool OnIoPortAccessExit( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_IO_PORT_ACCESS_CONTEXT* ExitContext ) {
	printf( "IoPortAccessCallback" );

	// Get a pointer to registers
	//
	auto registers = Partition->VirtualProcessor.Registers;

	// Check which instruction is currently executed
	//
	auto port = ExitContext->PortNumber;
	if ( ExitContext->AccessInfo.IsWrite == 0 && port == 0x1f0 ) {
		// EC	IN AL,DX	Input byte from I/O port in DX into AL.
		//
		registers[ Rax ].Reg64 = static_cast< uint8_t >( 0x12 );
	} else if ( ExitContext->AccessInfo.IsWrite == 1 && port == 0x1f0 ) {
		// EE	OUT DX, AL	Output byte in AL to I/O port address in DX.
		//
		registers[ Rax ].Reg64 = static_cast< uint8_t >( 0x1337 );
	}
	else
		return false;

	// Adjust rip ( ONLY FOR TEST )
	registers[ Rip ].Reg64 += 1;

	if ( FAILED( WhSeSetProcessorRegisters( Partition, registers ) ) )
		return false;

	return true;
}

bool OnUnrecoverableExceptionExit( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_UNRECOVERABLE_EXCEPTION_CONTEXT* ExitContext ) {
	printf( "UnrecoverableExceptionCallback" );

	auto idtr = Partition->VirtualProcessor.Registers[ Idtr ].Table;

	WHSE_ALLOCATION_NODE* node = nullptr;
	auto hresult = WhSeFindAllocationNodeByGva(Partition, idtr.Base, &node);
	if ( FAILED( hresult ) )
		return true;

	uintptr_t gpa;
	WHV_TRANSLATE_GVA_RESULT tr { };
	hresult = WhSeTranslateGvaToGpa( Partition, reinterpret_cast< uintptr_t >( node->GuestVirtualAddress ) + 0x1000, &gpa, &tr );
	if ( FAILED( hresult ) )
		return true;

	return false;
}

bool OnInvalidRegisterValueExit( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_INVALID_REGISTER_VALUE_CONTEXT* ExitContext ) {
	printf( "InvalidRegisterValueCallback" );
	return false;
}

bool OnUnsupportedFeatureExit( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_UNSUPPORTED_FEATURE_CONTEXT* ExitContext ) {
	printf( "UnsupportedFeatureCallback" );
	return false;
}

bool OnInterruptionDeliveryExit( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_INTERRUPTION_DELIVERY_CONTEXT* ExitContext ) {
	printf( "InterruptionDeliveryCallback" );
	return false;
}

bool OnHaltExit( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_HALT_CONTEXT* ExitContext ) {
	printf( "HaltCallback" );
	return false;
}

bool OnApicEoiExit( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_APIC_EOI_CONTEXT* ExitContext ) {
	printf( "ApicEoiCallback" );
	return false;
}

bool OnMsrAccessExit( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_MSR_ACCESS_CONTEXT* ExitContext ) {
	printf( "MsrAccessCallback" );
	return false;
}

bool OnCpuidAccessExit( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_CPUID_ACCESS_CONTEXT* ExitContext ) {
	printf( "CpuidAccessCallback" );
	return false;
}

bool OnVirtualProcessorExit( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_VP_EXCEPTION_CONTEXT* ExitContext ) {
	printf( "VirtualProcessorCallback" );
	return false;
}

bool OnRdtscAccessExit( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_RDTSC_ACCESS_CONTEXT* ExitContext ) {
	printf( "RdtscAccessCallback" );
	return false;
}

bool OnUserCanceledExit( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_USER_CANCELED_CONTEXT* ExitContext ) {
	printf( "UserCanceledCallback" );
	return false;
}

// Handle virtual processor exit reason
//
bool OnExit( WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, void* ExitContext ) {
	auto ctx = Partition->VirtualProcessor.ExitContext;
	auto rip = Partition->VirtualProcessor.Registers[ Rip ].Reg64;
	auto rsp = Partition->VirtualProcessor.Registers[ Rsp ].Reg64;
	auto cs = Partition->VirtualProcessor.Registers[ Cs ].Segment;
	auto ss = Partition->VirtualProcessor.Registers[ Ss ].Segment;
	
	// Do not retry
	//
	return false; 
}

// Execute a shellcode through a virtual processor
//
DWORD WINAPI ExecuteThread( LPVOID lpParameter ) {
	auto params = reinterpret_cast< RUN_PARAMS* >( lpParameter );

	auto partition = params->Partition;

	// Initialize the processor state
	//
	auto registers = partition->VirtualProcessor.Registers;

	if ( FAILED( WhSeGetProcessorRegisters( partition, registers ) ) )
		return -1;

	// Set Entry Point
	//
	registers[ Rip ].Reg64 = params->Entrypoint;

	// Set Stack Pointer
	//
	registers[ Rsp ].Reg64 = params->Stack;

	if ( FAILED( WhSeSetProcessorRegisters( partition, registers ) ) )
		return -1;

	// Set exit callbacks
	//
	partition->ExitCallbacks.u.MemoryAccessCallback = &OnMemoryAccessExit;
	partition->ExitCallbacks.u.IoPortAccessCallback = &OnIoPortAccessExit;
	partition->ExitCallbacks.u.UnrecoverableExceptionCallback = &OnUnrecoverableExceptionExit;
	partition->ExitCallbacks.u.InvalidRegisterValueCallback = &OnInvalidRegisterValueExit;
	partition->ExitCallbacks.u.UnsupportedFeatureCallback = &OnUnsupportedFeatureExit;
	partition->ExitCallbacks.u.InterruptionDeliveryCallback = &OnInterruptionDeliveryExit;
	partition->ExitCallbacks.u.HaltCallback = &OnHaltExit;
	partition->ExitCallbacks.u.ApicEoiCallback = &OnApicEoiExit;
	partition->ExitCallbacks.u.MsrAccessCallback = &OnMsrAccessExit;
	partition->ExitCallbacks.u.CpuidAccessCallback = &OnCpuidAccessExit;
	partition->ExitCallbacks.u.VirtualProcessorCallback = &OnVirtualProcessorExit;
	partition->ExitCallbacks.u.RdtscAccessCallback = &OnRdtscAccessExit;
	partition->ExitCallbacks.u.UserCanceledCallback = &OnUserCanceledExit;

	// *---------------*
	// | START TESTING |
	// *---------------*

	// *---------------------------------------------------------------------------------------------------------------------------*
	// | Allocate VA then free it, it will add the VA to the PT but unmap the backing page, this will trigger a memory access exit |
	// *---------------------------------------------------------------------------------------------------------------------------*
	/*uintptr_t gva = 0xdeadbeef;
	PVOID hva = nullptr;
	size_t sz = 0x1000;
	WhSeAllocateGuestVirtualMemory( partition, &hva, gva, &sz, WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite );

	WhSeFreeGuestVirtualMemory( partition, hva, gva, sz );*/

	// *---------------*
	// |  END TESTING  |
	// *---------------*

	// Main guest execution loop
	//
	HRESULT hresult = S_OK;
	for ( ;; ) {
		WHSE_VP_EXIT_REASON exitReason;
		if ( FAILED( hresult = WhSeRunProcessor( partition, &exitReason ) ) )
			break;

		if ( exitReason != WHvRunVpExitReasonNone )
			break;
	}

	return static_cast< DWORD >( hresult );
}

DWORD Cleanup( WHSE_PARTITION** Partition ) {
	// Release the backing memory from the PML4 directory
	//
	auto pml4Hva = ( *Partition )->MemoryLayout.Pml4HostVa;
	if ( pml4Hva == nullptr )
		EXIT_WITH_MESSAGE( "No PML4 HVA" );

	::VirtualFree( pml4Hva, 0, MEM_RELEASE );

	// Delete the processor
	//
	if ( FAILED( WhSeDeleteProcessor( *Partition ) ) )
		EXIT_WITH_MESSAGE( "Failed to delete virtual processor" );

	// Delete the partition
	//
	if ( FAILED( WhSeDeletePartition( Partition ) ) )
		EXIT_WITH_MESSAGE( "Failed to delete hypervisor partition" );

	printf( "Deleted hypervisor partition\n" );

	return EXIT_SUCCESS;
}

// Execute a shellcode through a virtual processor
//
DWORD WINAPI Run( const RUN_OPTIONS& options ) {
	// Create partition
	//
	WHSE_PARTITION* partition = nullptr;
	if ( FAILED( WhSeCreatePartition( &partition ) ) )
		EXIT_WITH_MESSAGE( "Failed to create hypervisor partition" );

	printf( "Hypervisor partition created\n" );
	printf( "Partition = %p\n", reinterpret_cast< void* >( partition ) );
	printf( "Partition->Handle = %p\n", reinterpret_cast< void* >( partition->Handle ) );

	// Create the processor
	// 
	if ( FAILED( WhSeCreateProcessor( partition, options.Mode ) ) )
		EXIT_WITH_MESSAGE( "Failed to create the processor" );

	printf( "Created processor %d\n", partition->VirtualProcessor.Index );

	// Initialize paging
	//
	if ( FAILED( WhSeInitializeMemoryLayout( partition ) ) )
		EXIT_WITH_MESSAGE( "Failed to initialize memory layout" );

	printf( "Initialized paging (CR3 = %llx)\n", static_cast< unsigned long long >( partition->MemoryLayout.Pml4PhysicalAddress ) );
	
	// Allocate stack
	//
	PVOID stackHva = nullptr;
	constexpr uintptr_t stackGva = 0x00007FFF'00000000 - 0x1000000;
	size_t stackSize = 1 * 1024 * 1024;
	if ( FAILED( WhSeAllocateGuestVirtualMemory( partition, &stackHva, stackGva, &stackSize, WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite ) ) )
		EXIT_WITH_MESSAGE( "Failed to allocate stack" );

	printf( "Allocated stack space %llx (size = %llx)\n", static_cast< unsigned long long >( stackGva ), static_cast< unsigned long long >( stackSize ) );

	// Allocate code
	//
	auto shellcode = ::VirtualAlloc( nullptr, ALIGN_UP( options.CodeSize ), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );
	if ( shellcode == nullptr )
		EXIT_WITH_MESSAGE( "Failed to allocate shellcode backing memory (host)" );

	::CopyMemory( shellcode, options.Code, options.CodeSize );

	uintptr_t codeGva = options.BaseAddress != 0 ? options.BaseAddress : 0x10000;
	if( FAILED( WhSeMapHostToGuestVirtualMemory( partition, shellcode, codeGva, ALIGN_UP( options.CodeSize ), WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite | WHvMapGpaRangeFlagExecute ) ) )
		EXIT_WITH_MESSAGE( "Failed to map shellcode" );

	printf( "Allocated code memory %llx (size = %llx, allocated = %llx)\n", static_cast< unsigned long long >( codeGva ), static_cast< unsigned long long >( options.CodeSize ), static_cast< unsigned long long >( ALIGN_UP( options.CodeSize ) ) );

	// Run the processor
	//
	RUN_PARAMS params {
		.Entrypoint = codeGva,
		.Stack = stackGva + stackSize - PAGE_SIZE, // set rsp to the end of the allocated stack range as stack "grows downward" (let 1 page on top for "safety")
		.Partition = partition
	};

	// test code
	// alloc deadbeef va
	// free deadbeef va
	// test code

	printf( "Starting the processor ...\n" );

	auto thread = ::CreateThread( nullptr, 0, ExecuteThread, &params, 0, nullptr );
	if ( thread == nullptr )
		EXIT_WITH_MESSAGE( "Failed to start the processor thread" );

	// Wait until execution finishes or an unhandled vcpu exit
	//
	::WaitForSingleObject( thread, INFINITE );

	uint32_t exitCode;
	::GetExitCodeThread( thread, reinterpret_cast< LPDWORD >( &exitCode ) );
	printf( "Run thread exited with reason %d\n", exitCode );

	::CloseHandle( thread );

	// Cleanup
	//
	return Cleanup( &partition );
}
