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

/** @file
 * @brief This file expose the structure used in the library
 *
 */


typedef WHV_MAP_GPA_RANGE_FLAGS WHSE_MEMORY_ACCESS_FLAGS;

typedef WHV_REGISTER_NAME WHSE_REGISTER_NAME;
typedef WHV_REGISTER_VALUE WHSE_REGISTER_VALUE;

typedef WHV_RUN_VP_EXIT_CONTEXT WHSE_VP_EXIT_CONTEXT;
typedef WHV_RUN_VP_EXIT_REASON WHSE_VP_EXIT_REASON;

/**
 * @brief Enumeration to represent registers
 */
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

/**
 * @brief A structure representing a virtual processor
 *
 * This structure represent a virtual processor, its index (number),
 * the exit context upon a virtual processor exit and its registers state
 */
typedef struct _WHSE_VIRTUAL_PROCESSOR {
	uint32_t Index;
	WHSE_VP_EXIT_CONTEXT ExitContext;
	WHSE_REGISTERS Registers;
} WHSE_VIRTUAL_PROCESSOR, * PWHSE_VIRTUAL_PROCESSOR;

/**
 * @brief A structure to represent a guest memory allocation
 *
 * This structure a guest memory allocation.
 * In case of physical memory allocation, the <GuestVirtualAddress> field is set to null and
 * the <GuestPhysicalAddress> field is set with the guest physical memory address.
 * In case of virtual memory allocation, the <GuestVirtualAddress> field is set to the
 * guest address space virtual address and the <GuestPhysicalAddress> field is set with
 * the guest physical memory address backing this virtual address.
 * In either case the <HostVirtualAddress> is set to the host address space virtual address
 * that is backing the allocated guest physical address space. The <Size> field must be set
 * to the size that represent the allocated memory ranges (and must be non zero).
 */
typedef struct _WHSE_ALLOCATION_NODE : SLIST_ENTRY {
	PVOID GuestVirtualAddress;
	uintptr_t GuestPhysicalAddress;
	PVOID HostVirtualAddress;
	size_t Size;
} WHSE_ALLOCATION_NODE, * PWHSE_ALLOCATION_NODE;

/**
 * @brief A structure to represent the boundaries of address space
 */
typedef struct _WHSE_ADDRESS_SPACE_BOUNDARY {
	uintptr_t LowestAddress;
	uintptr_t HighestAddress;
	size_t SizeInBytes;
} WHSE_ADDRESS_SPACE_BOUNDARY, * PWHSE_ADDRESS_SPACE_BOUNDARY;

/**
 * @brief A structure to store the memory layout
 *
 * The structure holds the physical memory and virtual memory boundaries.
 * The structure holds a list of host memory allocations backing the physical guest memory.
 * Paging directories guest physical address and host address is available throught <Pml4PhysicalAddress>
 * and <Pml4HostVa> properties.
 */
typedef struct _WHSE_MEMORY_LAYOUT_DESC {
	WHSE_ADDRESS_SPACE_BOUNDARY PhysicalAddressSpace;
	WHSE_ADDRESS_SPACE_BOUNDARY VirtualAddressSpace;
	uintptr_t Pml4PhysicalAddress;
	PVOID Pml4HostVa;
	PSLIST_HEADER AllocationTracker;
} WHSE_MEMORY_LAYOUT_DESC, * PWHSE_MEMORY_LAYOUT_DESC;

typedef WHV_PARTITION_HANDLE WHSE_PARTITION_HANDLE;

/**
 * @brief Enumeration to represent the virtual processor exits callbacks
 */
enum WHSE_EXIT_CALLBACK_SLOT : uint8_t {
	// Standard exits caused by operations of the virtual processor
	//
	MemoryAccess,
	IoPortAccess,
	UnrecoverableException,
	InvalidVpRegisterValue,
	UnsupportedFeature,
	InterruptWindow,
	Halt,
	ApicEoi,

	// Additional exits that can be configured through partition properties
	//
	MsrAccess,
	Cpuid,
	Exception,
	Rdtsc,

	// Exits caused by the host
	//
	UserCanceled,

	NumberOfCallbacks
};

#define WHSECALLBACKAPI *
#define WHSE_CALLBACK_RETURNTYPE bool

typedef void * WHSE_CALLBACK;

struct _WHSE_PARTITION;

typedef WHV_MEMORY_ACCESS_CONTEXT WHSE_MEMORY_ACCESS_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_MEMORYACCESS_CALLBACK )( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_MEMORY_ACCESS_CONTEXT* Context );

typedef WHV_X64_IO_PORT_ACCESS_CONTEXT WHSE_IO_PORT_ACCESS_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_IO_PORT_ACCESS_CALLBACK )( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_IO_PORT_ACCESS_CONTEXT* Context );

typedef void WHSE_UNRECOVERABLE_EXCEPTION_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_UNRECOVERABLE_EXCEPTION_CALLBACK )( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_UNRECOVERABLE_EXCEPTION_CONTEXT* Context );

typedef void WHSE_INVALID_REGISTER_VALUE_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_INVALID_REGISTER_VALUE_CALLBACK )( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_INVALID_REGISTER_VALUE_CONTEXT* Context );

typedef WHV_X64_UNSUPPORTED_FEATURE_CONTEXT WHSE_UNSUPPORTED_FEATURE_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_UNSUPPORTED_FEATURE_CALLBACK )( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_UNSUPPORTED_FEATURE_CONTEXT* Context );

typedef WHV_X64_INTERRUPTION_DELIVERABLE_CONTEXT WHSE_INTERRUPTION_DELIVERY_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_INTERRUPTION_DELIVERY_CALLBACK )( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_INTERRUPTION_DELIVERY_CONTEXT* Context );

typedef void WHSE_HALT_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_HALT_CALLBACK )( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_HALT_CONTEXT* Context );

typedef WHV_X64_APIC_EOI_CONTEXT WHSE_APIC_EOI_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_APIC_EOI_CALLBACK )( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_APIC_EOI_CONTEXT* Context );

typedef WHV_X64_MSR_ACCESS_CONTEXT WHSE_MSR_ACCESS_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_MSR_ACCESS_CALLBACK )( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_MSR_ACCESS_CONTEXT* Context );

typedef WHV_X64_CPUID_ACCESS_CONTEXT WHSE_CPUID_ACCESS_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_CPUID_ACCESS_CALLBACK )( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_CPUID_ACCESS_CONTEXT* Context );

typedef WHV_VP_EXCEPTION_CONTEXT WHSE_VP_EXCEPTION_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_VP_EXCEPTION_CALLBACK )( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_VP_EXCEPTION_CONTEXT* Context );

typedef void WHSE_RDTSC_ACCESS_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_RDTSC_ACCESS_CALLBACK )( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_RDTSC_ACCESS_CONTEXT* Context );

typedef WHV_RUN_VP_CANCELED_CONTEXT WHSE_USER_CANCELED_CONTEXT;
typedef WHSE_CALLBACK_RETURNTYPE ( WHSECALLBACKAPI WHSE_EXIT_USER_CANCELED_CALLBACK )( _WHSE_PARTITION* Partition, WHV_VP_EXIT_CONTEXT* VpContext, WHSE_USER_CANCELED_CONTEXT* Context );

/**
 * @brief A structure to hold pointers to the virtual processor exits callbacks
 */
typedef union _WHSE_EXIT_CALLBACKS {
	struct {
		WHSE_EXIT_MEMORYACCESS_CALLBACK				MemoryAccessCallback;
		WHSE_EXIT_IO_PORT_ACCESS_CALLBACK			IoPortAccessCallback;
		WHSE_EXIT_UNRECOVERABLE_EXCEPTION_CALLBACK	UnrecoverableExceptionCallback;
		WHSE_EXIT_INVALID_REGISTER_VALUE_CALLBACK	InvalidRegisterValueCallback;
		WHSE_EXIT_UNSUPPORTED_FEATURE_CALLBACK		UnsupportedFeatureCallback;
		WHSE_EXIT_INTERRUPTION_DELIVERY_CALLBACK	InterruptionDeliveryCallback;
		WHSE_EXIT_HALT_CALLBACK						HaltCallback;
		WHSE_EXIT_APIC_EOI_CALLBACK					ApicEoiCallback;
		WHSE_EXIT_MSR_ACCESS_CALLBACK				MsrAccessCallback;
		WHSE_EXIT_CPUID_ACCESS_CALLBACK				CpuidAccessCallback;
		WHSE_EXIT_VP_EXCEPTION_CALLBACK				VirtualProcessorCallback;
		WHSE_EXIT_RDTSC_ACCESS_CALLBACK				RdtscAccessCallback;
		WHSE_EXIT_USER_CANCELED_CALLBACK			UserCanceledCallback;
	} u;

	PVOID Callbacks[ WHSE_EXIT_CALLBACK_SLOT::NumberOfCallbacks ];
} WHSE_EXIT_CALLBACKS, * PWHSE_EXIT_CALLBACKS;

static_assert( sizeof( WHSE_EXIT_CALLBACKS::u ) == sizeof( uintptr_t ) * ( WHSE_EXIT_CALLBACK_SLOT::NumberOfCallbacks ), "Wrong WHSE_EXIT_CALLBACKS::u size" );
static_assert( sizeof( WHSE_EXIT_CALLBACKS::Callbacks ) == sizeof( uintptr_t ) * ( WHSE_EXIT_CALLBACK_SLOT::NumberOfCallbacks ), "WHSE_EXIT_CALLBACKS::Callbacks size" );
static_assert( sizeof( WHSE_EXIT_CALLBACKS::Callbacks ) == sizeof( WHSE_EXIT_CALLBACKS::u ), "Wrong WHSE_EXIT_CALLBACKS::Callbacks or WHSE_EXIT_CALLBACKS::u size" );

/**
 * @brief A structure to represent a partition
 *
 * A partition is the highest level structure and holds everything needed
 * to run code on a virtual processor.
 * The <Handle> property is an opaque pointer to a <WHV_PARTITION> structure.
 * The <MemoryLayout> holds the properties of the memory layout.
 * The <VirtualProcessor> holds the properties of a virtual processor.
 * The <ExitCallbacks> hold an array of pointers to the virtual processor exits callbacks.
 */
typedef struct _WHSE_PARTITION {
	WHSE_PARTITION_HANDLE Handle;
	WHSE_MEMORY_LAYOUT_DESC MemoryLayout;
	WHSE_VIRTUAL_PROCESSOR VirtualProcessor;
	WHSE_EXIT_CALLBACKS ExitCallbacks;
} WHSE_PARTITION, * PWHSE_PARTITION;

#endif // !WINHVDEFS_HPP