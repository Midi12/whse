#include "Executor.hpp"
#include "Utils.hpp"

#include "winbase.h"
#include "winnt.h"

#include <windows.h>
#include <winhvplatform.h>

#include <stdio.h>


#include <vector>

#define EXIT_WITH_MESSAGE( x ) \
	{ \
		printf( x "\n" ); \
		printf( "Last hresult = %llx\n", static_cast< unsigned long long >( WhSeGetLastHresult() ) ); \
		printf( "Last error code = %llx\n", static_cast< unsigned long long >( WhSeGetLastError() ) ); \
		return EXIT_FAILURE; \
	} \

constexpr size_t PAGE_SIZE = 4096;

constexpr size_t ALIGN_PAGE( size_t x ) {
	return ( ( PAGE_SIZE - 1 ) & x ) ? ( ( x + PAGE_SIZE ) & ~( PAGE_SIZE - 1 ) ) : x;
}

#define MAKE_SEGMENT(reg, sel, base, limit, dpl, l, gr)			\
	registers[ reg ].Segment.Selector = ( sel | dpl );			\
	registers[ reg ].Segment.Base = base;						\
	registers[ reg ].Segment.Limit = limit;						\
	registers[ reg ].Segment.Present = 1;						\
	registers[ reg ].Segment.DescriptorPrivilegeLevel = dpl;	\
	registers[ reg ].Segment.Long = l;							\
	registers[ reg ].Segment.Granularity = gr

#define MAKE_CR0(pe, mp, em, ts, et, ne, wp, am, nw, cd, pg)	\
	(															\
		(														\
			( pg << 31 ) |										\
			( cd << 30 ) |										\
			( nw << 29 ) |										\
			( am << 18 ) |										\
			( wp << 16 ) |										\
			( ne << 5 ) |										\
			( et << 4 ) |										\
			( ts << 3 ) |										\
			( em << 2 ) |										\
			( mp << 1 ) |										\
			( pe << 0 )											\
		) & UINT32_MAX											\
	)

#define MAKE_CR4(vme, pvi, tsd, de, pse, pae, mce, pge, pce, osfxsr, osxmmexcpt, umip, la57, vmxe, smxe, fsgsbase, pcide, osxsave, smep, smap, pke, cet, pks) \
	(															\
		(														\
			( pks			<< 24 ) |							\
			( cet			<< 23 ) |							\
			( pke			<< 22 ) |							\
			( smap			<< 21 ) |							\
			( smep			<< 20 ) |							\
			( osxsave		<< 18 ) |							\
			( pcide			<< 17 ) |							\
			( fsgsbase		<< 16 ) |							\
			( smxe			<< 14 ) |							\
			( vmxe			<< 13 ) |							\
			( la57			<< 12 ) |							\
			( umip			<< 11 ) |							\
			( osxmmexcpt	<< 10 ) |							\
			( osfxsr		<< 9 ) |							\
			( pce			<< 8 ) |							\
			( pge			<< 7 ) |							\
			( mce			<< 6 ) |							\
			( pae			<< 5 ) |							\
			( pse			<< 4 ) |							\
			( de			<< 3 ) |							\
			( tsd			<< 2 ) |							\
			( pvi			<< 1 ) |							\
			( vme			<< 0 )								\
		) & ( ~( 1 << 24 ) )									\
	)

#define MAKE_EFER(sce, dpe, sewbed, gewbed, l2d, lme, lma, nxe, svme, lmsle, ffxsr, tce) \
	(															\
		(														\
			( tce		<< 15) |								\
			( ffxsr		<< 14) |								\
			( lmsle		<< 13) |								\
			( svme		<< 12) |								\
			( nxe		<< 11) |								\
			( lma		<< 10) |								\
			( lme		<< 8) |									\
			( l2d		<< 4) |									\
			( gewbed	<< 3) |									\
			( sewbed	<< 2) |									\
			( dpe		<< 1) |									\
			( sce		<< 0 )									\
		) & ( ~( 1 << 16 ) )									\
	)

#define MAKE_RFLAGS(cf, pf, af, zf, sf, tf, if_, df, of, iopl, nt, rf, vm, ac, vif, vip, id) \
	(															\
		(														\
			( id	<< 21 ) |									\
			( vip	<< 20 ) |									\
			( vif	<< 19 ) |									\
			( ac	<< 18 ) |									\
			( vm	<< 17 ) |									\
			( rf	<< 16 ) |									\
			( nt	<< 14 ) |									\
			( iopl	<< 12 ) |									\
			( of	<< 11 ) |									\
			( df	<< 10 ) |									\
			( if_	<< 9 ) |									\
			( tf	<< 8 ) |									\
			( sf	<< 7 ) |									\
			( zf	<< 6 ) |									\
			( af	<< 4 ) |									\
			( pf	<< 2 ) |									\
			(  1	<< 1 ) |									\
			( cf	<< 0 )										\
		) & ( ~( 1 << 21 ) )									\
	)

// Handle virtual processor exit reason
//
HRESULT HandleExitReason( WHSE_PARTITION* Partition, WHSE_VP_EXIT_REASON ExitReason ) {
	auto hresult = ( HRESULT ) -1;

	// Handle exit reason
	//
	switch ( ExitReason )
	{
		case WHvRunVpExitReasonNone:
		case WHvRunVpExitReasonCanceled:
			hresult = S_OK;
			break;
		case WHvRunVpExitReasonMemoryAccess:
			break;
		case WHvRunVpExitReasonX64IoPortAccess:
			break;
		case WHvRunVpExitReasonUnrecoverableException:
		{
			auto ctx = Partition->VirtualProcessor.ExitContext;
			auto rip = Partition->VirtualProcessor.Registers[ Rip ].Reg64;
			auto rsp = Partition->VirtualProcessor.Registers[ Rsp ].Reg64;
			auto cs = Partition->VirtualProcessor.Registers[ Cs ].Segment;
			auto ss = Partition->VirtualProcessor.Registers[ Ss ].Segment;

			auto a = 1;
		}
			break;
		case WHvRunVpExitReasonInvalidVpRegisterValue:
			break;
		case WHvRunVpExitReasonUnsupportedFeature:
			break;
		case WHvRunVpExitReasonX64InterruptWindow:
			break;
		case WHvRunVpExitReasonX64Halt:
			break;
		case WHvRunVpExitReasonX64MsrAccess:
			break;
		case WHvRunVpExitReasonX64Cpuid:
			break;
		case WHvRunVpExitReasonException:
			break;
		case WHvRunVpExitReasonX64Rdtsc:
			break;
		default:
			break;
	}
	
	return hresult;
}

// Execute a shellcode through a virtual processor
//
DWORD WINAPI ExecuteThread( LPVOID lpParameter ) {
	auto params = reinterpret_cast< EXECUTOR_PARAMS* >( lpParameter );

	auto partition = params->Partition;

	// Initialize the processor state
	//
	auto registers = partition->VirtualProcessor.Registers;

	if ( FAILED( WhSeGetProcessorRegisters( partition, registers ) ) )
		return -1;

	int ring;
	int codeSelector;
	int dataSelector;

	// Setup processor
	//
	switch ( params->Mode ) {
		using enum PROCESSOR_MODE;
		
		// Setup processor state for kernel mode 
		//
		case KernelMode:
			{
				ring = 0;
				codeSelector = 0x10;
				dataSelector = 0x18;
			}
			break;

		// Setup processor state for user mode
		//
		case UserMode:
			{
				ring = 3;
				codeSelector = 0x30;
				dataSelector = 0x28;
			}
			break;

		// Unknown layout : exit
		//
		default:
			return -1;
	}

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

	registers[ Rip ].Reg64 = params->Entrypoint;
	registers[ Rsp ].Reg64 = params->Stack;

	// Set IF bit
	//
	registers[ Rflags ].Reg64 = MAKE_RFLAGS( 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );

	if ( FAILED( WhSeSetProcessorRegisters( partition, registers ) ) )
		return -1;

	// Main guest execution loop
	//
	HRESULT hresult = S_OK;
	for ( ;; ) {
		// todo : add mutex to break

		WHSE_VP_EXIT_REASON exitReason;
		if ( FAILED( hresult = WhSeRunProcessor( partition, &exitReason ) ) )
			break;

		if ( FAILED( hresult = HandleExitReason( partition, exitReason ) ) )
			break;
	}

	return static_cast< DWORD >( hresult );
}

DWORD Cleanup( WHSE_PARTITION** Partition ) {
	// Release all allocation on backing host memory
	//
	auto tracker = ( *Partition )->MemoryLayout.AllocationTracker;
	if ( tracker != nullptr ) {
		auto first = reinterpret_cast< WHSE_ALLOCATION_NODE* >( ::RtlFirstEntrySList( tracker ) );
		if ( first == nullptr )
			EXIT_WITH_MESSAGE( "No element in the VA Node DB" );

		auto current = first;
		while ( current != nullptr ) {
			::VirtualFree( current->HostVirtualAddress, 0, MEM_RELEASE );

			current = reinterpret_cast< WHSE_ALLOCATION_NODE* >( current->Next );
		}
	}

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
DWORD WINAPI Execute( const EXECUTOR_OPTIONS& options ) {
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
	if ( FAILED( WhSeCreateProcessor( partition ) ) )
		EXIT_WITH_MESSAGE( "Failed to create the processor" );

	printf( "Created processor %d\n", partition->VirtualProcessor.Index );

	// Initialize paging
	//
	if ( FAILED( WhSeInitializeMemoryLayout( partition ) ) )
		EXIT_WITH_MESSAGE( "Failed to initialize memory layout" );

	printf( "Initialized paging (CR3 = %llx)\n", static_cast< unsigned long long >( partition->MemoryLayout.Pml4PhysicalAddress ) );

	uintptr_t lowestAddress = 0;
	uintptr_t highestAddress = 0;

	// Setup Virtual Address Space boundaries
	//
	switch ( options.Mode ) {
		using enum PROCESSOR_MODE;
		case UserMode:
			lowestAddress = 0x00000000'00000000;
			highestAddress = 0x00008000'00000000 - 64KiB;
			break;
		case KernelMode:
			lowestAddress = 0xffff8000'00000000;
			highestAddress = 0xffffffff'ffffffff - 4MiB;
			break;
		default:
			EXIT_WITH_MESSAGE( "Unsupported mode" );
	}

	partition->MemoryLayout.VirtualAddressSpace.LowestAddress = lowestAddress;
	partition->MemoryLayout.VirtualAddressSpace.HighestAddress = highestAddress;
	partition->MemoryLayout.VirtualAddressSpace.SizeInBytes = highestAddress - lowestAddress;

	// Allocate stack
	//
	PVOID stackHva = nullptr;
	constexpr uintptr_t stackGva = 0x00007FFF'FFFF0000 - 0x1000000;
	size_t stackSize = 1 * 1024 * 1024;
	if ( FAILED( WhSeAllocateGuestVirtualMemory( partition, &stackHva, stackGva, &stackSize, WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite ) ) )
		EXIT_WITH_MESSAGE( "Failed to allocate stack" );

	printf( "Allocated stack space %llx (size = %llx)\n", static_cast< unsigned long long >( stackGva ), static_cast< unsigned long long >( stackSize ) );

	// Allocate code
	//
	auto shellcode = ::VirtualAlloc( nullptr, ALIGN_PAGE( options.CodeSize ), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );
	if ( shellcode == nullptr )
		EXIT_WITH_MESSAGE( "Failed to allocate shellcode backing memory (host)" );

	::CopyMemory( shellcode, options.Code, options.CodeSize );

	constexpr uintptr_t codeGva = 0x10000;
	if( FAILED( WhSeMapHostToGuestVirtualMemory( partition, shellcode, codeGva, ALIGN_PAGE( options.CodeSize ), WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite | WHvMapGpaRangeFlagExecute ) ) )
		EXIT_WITH_MESSAGE( "Failed to map shellcode" );

	printf( "Allocated code memory %llx (size = %llx, allocated = %llx)\n", static_cast< unsigned long long >( codeGva ), static_cast< unsigned long long >( options.CodeSize ), static_cast< unsigned long long >( ALIGN_PAGE( options.CodeSize ) ) );

	// Run the processor
	//
	EXECUTOR_PARAMS params {
		.Entrypoint = codeGva,
		.Stack = stackGva + stackSize - PAGE_SIZE, // set rsp to the end of the allocated stack range as stack "grows downward" (let 1 page on top for "safety")
		.Partition = partition,
		.Mode = options.Mode
	};

	printf( "Starting the processor ...\n" );

	auto thread = ::CreateThread( nullptr, 0, ExecuteThread, &params, 0, nullptr );
	if ( thread == nullptr )
		EXIT_WITH_MESSAGE( "Failed to start the processor thread" );

	// Wait until execution finishes or an unhandled vcpu exit
	//
	::WaitForSingleObject( thread, INFINITE );

	// debug code
	std::vector<uint8_t> codeBytes( reinterpret_cast< uint8_t* >( shellcode ), reinterpret_cast< uint8_t* >( reinterpret_cast< uintptr_t >( shellcode ) + options.CodeSize ) );
	std::vector<uint8_t> stackBytes( reinterpret_cast< uint8_t* >( stackHva ), reinterpret_cast< uint8_t* >( reinterpret_cast< uintptr_t >( stackHva ) + stackSize ) );

	auto it = std::find_if( std::begin( stackBytes ), std::end( stackBytes ), [] ( uint8_t b ) { return b != 0; } );
	bool is_end = it == std::end( stackBytes );

	// debug code

	uint32_t exitCode;
	::GetExitCodeThread( thread, reinterpret_cast< LPDWORD >( &exitCode ) );
	printf( "Run thread exited with reason %d\n", exitCode );

	::CloseHandle( thread );

	// Cleanup
	//
	return Cleanup( &partition );
}
