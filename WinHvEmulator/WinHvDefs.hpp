#ifndef WINHVDEFS_HPP
#define WINHVDEFS_HPP

#include <windows.h>
#include <winhvplatform.h>

#include <cstdint>

#if defined( WIN64 ) || defined( _WIN64 )
#define WINHVEMULATOR_64
#elif define ( WIN32 ) || defined( _WIN32 )
#define WINHVEMULATOR_32
#elif
#error "Unsupported Operating System"
#endif

typedef WHV_MAP_GPA_RANGE_FLAGS WHSE_MEMORY_ACCESS_FLAGS;

typedef WHV_REGISTER_NAME WHSE_REGISTER_NAME;
typedef WHV_REGISTER_VALUE WHSE_REGISTER_VALUE;

typedef WHV_RUN_VP_EXIT_CONTEXT WHSE_VP_EXIT_CONTEXT;
typedef WHV_RUN_VP_EXIT_REASON WHSE_VP_EXIT_REASON;

enum WHSE_REGISTER : uint8_t {
	Rax,	Rbx,	Rcx,	Rdx,
	Rbp,	Rsp,
	Rsi,	Rdi,
	R8,		R9,		R10,	R11,
	R12,	R13,	R14,	R15,
	Rip,
	Rflags,
	Gs,		Fs,		Es,
	Ds,		Cs,		Ss,
	Gdtr,	Ldtr,	Idtr,
	Cr0,	Cr2,	Cr3,	Cr4,
	Efer,

	NumberOfRegisters
};

static constexpr WHSE_REGISTER_NAME g_registers[] = {
	WHvX64RegisterRax,
	WHvX64RegisterRbx,
	WHvX64RegisterRcx,
	WHvX64RegisterRdx,
	WHvX64RegisterRbp,
	WHvX64RegisterRsp,
	WHvX64RegisterRsi,
	WHvX64RegisterRdi,
	WHvX64RegisterR8,
	WHvX64RegisterR9,
	WHvX64RegisterR10,
	WHvX64RegisterR11,
	WHvX64RegisterR12,
	WHvX64RegisterR13,
	WHvX64RegisterR14,
	WHvX64RegisterR15,
	WHvX64RegisterRip,
	WHvX64RegisterRflags,
	WHvX64RegisterGs,
	WHvX64RegisterFs,
	WHvX64RegisterEs,
	WHvX64RegisterDs,
	WHvX64RegisterCs,
	WHvX64RegisterSs,
	WHvX64RegisterGdtr,
	WHvX64RegisterLdtr,
	WHvX64RegisterIdtr,
	WHvX64RegisterCr0,
	WHvX64RegisterCr2,
	WHvX64RegisterCr3,
	WHvX64RegisterCr4,
	WHvX64RegisterEfer
};

static_assert( RTL_NUMBER_OF( g_registers ) == WHSE_REGISTER::NumberOfRegisters, "number of registers in g_registers != WHSE_REGISTER::NumberOfRegisters" );

static constexpr uint32_t g_registers_count = RTL_NUMBER_OF( g_registers );

using WHSE_REGISTERS = WHSE_REGISTER_VALUE[ g_registers_count ];

typedef struct WHSE_VIRTUAL_PROCESSOR {
	uint32_t Index;
	WHSE_VP_EXIT_CONTEXT ExitContext;
	WHSE_REGISTERS Registers;
};

typedef struct WHSE_ALLOCATION_NODE : SLIST_ENTRY {
	PVOID GuestVirtualAddress;
	uintptr_t GuestPhysicalAddress;
	PVOID HostVirtualAddress;
	size_t Size;
};

typedef struct WHSE_ADDRESS_SPACE_BOUNDARY {
	uintptr_t LowestAddress;
	uintptr_t HighestAddress;
	size_t SizeInBytes;
};

typedef struct WHSE_MEMORY_LAYOUT_DESC {
	WHSE_ADDRESS_SPACE_BOUNDARY PhysicalAddressSpace;
	uintptr_t Pml4PhysicalAddress;
	PVOID Pml4HostVa;
	PSLIST_HEADER AllocationTracker;
	WHSE_ADDRESS_SPACE_BOUNDARY VirtualAddressSpace;
};

typedef WHV_PARTITION_HANDLE WHSE_PARTITION_HANDLE;

enum WHSE_EXIT_CALLBACK_SLOT : uint8_t {
	// Standard exits caused by operations of the virtual processor
	MemoryAccess,
	IoPortAccess,
	UnrecoverableException,
	InvalidVpRegisterValue,
	UnsupportedFeature,
	InterruptWindow,
	Halt,
	ApicEoi,

	// Additional exits that can be configured through partition properties
	MsrAccess,
	Cpuid,
	Exception,
	Rdtsc,

	// Exits caused by the host
	UserCanceled,

	NumberOfCallbacks
};

#define WHSECALLBACKAPI *
#define WHSE_CALLBACK_RETURNTYPE bool

typedef void * WHSE_CALLBACK;

struct WHSE_PARTITION;

typedef WHV_MEMORY_ACCESS_CONTEXT WHSE_MEMORY_ACCESS_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_MEMORYACCESS_CALLBACK )( WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_MEMORY_ACCESS_CONTEXT* ExitContext );

typedef WHV_X64_IO_PORT_ACCESS_CONTEXT WHSE_IO_PORT_ACCESS_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_IO_PORT_ACCESS_CALLBACK )( WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_IO_PORT_ACCESS_CONTEXT* Context );

typedef void WHSE_UNRECOVERABLE_EXCEPTION_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_UNRECOVERABLE_EXCEPTION_CALLBACK )( WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_UNRECOVERABLE_EXCEPTION_CONTEXT* Context );

typedef void WHSE_INVALID_REGISTER_VALUE_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_INVALID_REGISTER_VALUE_CALLBACK )( WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_INVALID_REGISTER_VALUE_CONTEXT* Context );

typedef WHV_X64_UNSUPPORTED_FEATURE_CONTEXT WHSE_UNSUPPORTED_FEATURE_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_UNSUPPORTED_FEATURE_CALLBACK )( WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_UNSUPPORTED_FEATURE_CONTEXT* Context );

typedef WHV_X64_INTERRUPTION_DELIVERABLE_CONTEXT WHSE_INTERRUPTION_DELIVERY_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_INTERRUPTION_DELIVERY_CALLBACK )( WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_INTERRUPTION_DELIVERY_CONTEXT* Context );

typedef void WHSE_HALT_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_HALT_CALLBACK )( WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_HALT_CONTEXT* Context );

typedef WHV_X64_APIC_EOI_CONTEXT WHSE_APIC_EOI_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_APIC_EOI_CALLBACK )( WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_APIC_EOI_CONTEXT* Context );

typedef WHV_X64_MSR_ACCESS_CONTEXT WHSE_MSR_ACCESS_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_MSR_ACCESS_CALLBACK )( WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_MSR_ACCESS_CONTEXT* Context );

typedef WHV_X64_CPUID_ACCESS_CONTEXT WHSE_CPUID_ACCESS_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_CPUID_ACCESS_CALLBACK )( WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_CPUID_ACCESS_CONTEXT* Context );

typedef WHV_VP_EXCEPTION_CONTEXT WHSE_VP_EXCEPTION_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_VP_EXCEPTION_CALLBACK )( WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_VP_EXCEPTION_CONTEXT* Context );

typedef void WHSE_RDTSC_ACCESS_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_RDTSC_ACCESS_CALLBACK )( WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_RDTSC_ACCESS_CONTEXT* Context );

typedef WHV_RUN_VP_CANCELED_CONTEXT WHSE_USER_CANCELED_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_USER_CANCELED_CALLBACK )( WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_USER_CANCELED_CONTEXT* Context );

typedef union WHSE_EXIT_CALLBACKS {
	struct {
		WHSE_EXIT_MEMORYACCESS_CALLBACK				MemoryAccessCallback;
		WHSE_EXIT_IO_PORT_ACCESS_CALLBACK			IoPortAccessCallback;
		WHSE_EXIT_UNRECOVERABLE_EXCEPTION_CALLBACK	UnrecoverableExceptionCallback;
		WHSE_EXIT_INVALID_REGISTER_VALUE_CALLBACK	InvalidRegisterValueCallback;
		WHSE_EXIT_UNSUPPORTED_FEATURE_CALLBACK		UnsupportedFeatureCallback;
		WHSE_EXIT_INTERRUPTION_DELIVERY_CALLBACK	InterruptionDeliveryCallback;
		WHSE_EXIT_HALT_CALLBACK						HaltCallback;
		WHSE_EXIT_MSR_ACCESS_CALLBACK				MsrAccessCallback;
		WHSE_EXIT_CPUID_ACCESS_CALLBACK				CpuidAccessCallback;
		WHSE_EXIT_VP_EXCEPTION_CALLBACK				VirtualProcessorCallback;
		WHSE_EXIT_RDTSC_ACCESS_CALLBACK				RdtscAccessCallback;
		WHSE_EXIT_USER_CANCELED_CALLBACK			UserCanceledCallback;
	} u;
	
	PVOID Callbacks[ WHSE_EXIT_CALLBACK_SLOT::NumberOfCallbacks ];
};

static_assert( sizeof( WHSE_EXIT_CALLBACKS::u ) == sizeof( uintptr_t ) * ( WHSE_EXIT_CALLBACK_SLOT::NumberOfCallbacks - 1 ) );
static_assert( sizeof( WHSE_EXIT_CALLBACKS::Callbacks ) == sizeof( uintptr_t ) * ( WHSE_EXIT_CALLBACK_SLOT::NumberOfCallbacks ) );

typedef struct WHSE_PARTITION {
	WHSE_PARTITION_HANDLE Handle;
	WHSE_MEMORY_LAYOUT_DESC MemoryLayout;
	WHSE_VIRTUAL_PROCESSOR VirtualProcessor;
	WHSE_EXIT_CALLBACKS ExitCallbacks;
};

#endif // !WINHVDEFS_HPP