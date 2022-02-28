#include "Executor.hpp"

#include <windows.h>
#include <winhvplatform.h>

#include <iostream>

#define EXIT_WITH_MESSAGE( x ) \
	{ \
		std::cerr << x << std::endl; \
		std::cerr << "Last hresult = " << std::hex << ::WhSeGetLastHresult() << std::endl; \
		std::cerr << "Last error code = " << std::hex << ::WhSeGetLastError() << std::endl; \
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
		hresult = S_OK;
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

	// Setup processor
	//
	switch ( params->Mode ) {
		using enum PROCESSOR_MODE;
		/*
		// Setup processor state for kernel mode 
		//
		case KernelMode:
			break;
		*/

		// Setup processor state for user mode
		//
		case UserMode:

			// Setup segment registers
			//
			registers[ Cs ].Segment.Selector = 0x33;
			registers[ Cs ].Segment.DescriptorPrivilegeLevel = 3;
			registers[ Cs ].Segment.Long = 1;

			registers[ Ss ].Segment.Selector = 0x2B;
			registers[ Ss ].Segment.DescriptorPrivilegeLevel = 3;
			registers[ Ds ] = registers[ Ss ];
			registers[ Es ] = registers[ Ss ];
			registers[ Gs ] = registers[ Ss ];

			break;

		// Unknown layout : exit
		//
		default:
			return -1;
	}

	registers[ Rip ].Reg64 = params->Entrypoint;
	registers[ Rsp ].Reg64 = params->Stack;

	// if bit
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

// Execute a shellcode through a virtual processor
//
DWORD WINAPI Execute( const uint8_t* Shellcode, const size_t Size ) {
	// Create partition
	//
	WHSE_PARTITION* partition = nullptr;
	if ( FAILED( WhSeCreatePartition( &partition ) ) )
		EXIT_WITH_MESSAGE( "Failed to create hypervisor partition" );

	std::cout << "Hypervisor partition created " << std::endl;
	std::cout << "Partition = " << ( void* ) partition << std::endl;
	std::cout << "Partition->Handle = " << ( void* ) partition->Handle << std::endl;

	// Create the processor
	// 
	if ( FAILED( ::WhSeCreateProcessor( partition ) ) )
		EXIT_WITH_MESSAGE( "Failed to create the processor" );

	// Initialize paging
	//
	if ( FAILED( WhSeInitializeMemoryLayout( partition ) ) )
		EXIT_WITH_MESSAGE( "Failed to initialize memory layout" );

	// Allocate stack
	//
	PVOID stackHva = nullptr;
	constexpr uintptr_t stackGva = 0x00007FFF'FFFF0000 - 0x1000000;
	size_t stackSize = 1 * 1024 * 1024;
	if ( FAILED( WhSeAllocateGuestVirtualMemory( partition, &stackHva, stackGva, &stackSize, WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite ) ) )
		EXIT_WITH_MESSAGE( "Failed to allocate stack" );

	// Allocate code
	//
	auto shellcode = ::VirtualAlloc( nullptr, ALIGN_PAGE( Size ), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );
	if ( shellcode == nullptr )
		EXIT_WITH_MESSAGE( "Failed to allocate shellcode" );

	::CopyMemory( shellcode, Shellcode, Size );

	constexpr uintptr_t codeGva = 0x10000;
	if( FAILED( WhSeMapHostToGuestVirtualMemory( partition, shellcode, codeGva, ALIGN_PAGE( Size ), WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite | WHvMapGpaRangeFlagExecute ) ) )
		EXIT_WITH_MESSAGE( "Failed to map shellcode" );

	std::cout << "Created processor " << partition->VirtualProcessor.Index << std::endl;

	// Run the processor
	//
	EXECUTOR_PARAMS params {
		.Entrypoint = codeGva,
		.Stack = stackGva,
		.Partition = partition,
		.Mode = PROCESSOR_MODE::UserMode
	};

	std::cout << "Starting the processor ..." << std::endl;

	auto thread = ::CreateThread( nullptr, 0, ExecuteThread, &params, 0, nullptr );
	if ( thread == nullptr )
		EXIT_WITH_MESSAGE( "Failed to start the processor thread" );

	// Wait until
	::WaitForSingleObject( thread, INFINITE );
	
	uint32_t exitCode;
	::GetExitCodeThread( thread, reinterpret_cast< LPDWORD >( &exitCode ) );
	std::cout << "Run thread exited with reason " << exitCode << std::endl;

	::CloseHandle( thread );

	// Cleanup partition
	//
	if ( FAILED( ::WhSeDeletePartition( partition ) ) )
		EXIT_WITH_MESSAGE( "Failed to delete hypervisor partition" );

	std::cout << "Deleted hypervisor partition" << std::endl;

	return EXIT_SUCCESS;
}
