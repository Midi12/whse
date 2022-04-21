#ifndef WINHVDEFS_HPP
#define WINHVDEFS_HPP

#include "DoubleLinkedList.hpp"

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
 * @brief Enumeration describing the processor mode
 */
enum WHSE_PROCESSOR_MODE : uint8_t {
	None,
	KernelMode,
	UserMode,

	NumberOfModes
};

typedef WHV_PROCESSOR_VENDOR WHSE_PROCESSOR_VENDOR;

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
	Gdtr,	Ldtr,	Idtr,	Tr,
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
	WHvX64RegisterTr,
	WHvX64RegisterCr0,
	WHvX64RegisterCr2,
	WHvX64RegisterCr3,
	WHvX64RegisterCr4,
	WHvX64RegisterEfer
};

static_assert( RTL_NUMBER_OF( g_registers ) == WHSE_REGISTER::NumberOfRegisters, "number of registers in g_registers != WHSE_REGISTER::NumberOfRegisters" );

static constexpr uint32_t g_registers_count = RTL_NUMBER_OF( g_registers );

using WHSE_REGISTERS = WHSE_REGISTER_VALUE[ g_registers_count ];

#define X64_TASK_STATE_SEGMENT_NUMBER_OF_ISTS 7
#define X64_TASK_STATE_SEGMENT_NUMBER_OF_SPS 3

#pragma pack( push, 1 )
struct _X64_TASK_STATE_SEGMENT {
	uint32_t Reserved00;

	// The Stack pointers used to load the stack on a privilege level change
	// (from a lower privileged ring to a higher one)
	//
	union {
		struct {
			uint64_t Rsp0;
			uint64_t Rsp1;
			uint64_t Rsp2;
		};
		uint64_t Rsp[ X64_TASK_STATE_SEGMENT_NUMBER_OF_SPS ];
	};

	uint32_t Reserved1C;
	uint32_t Reserved20;

	// Interrupt Stack Table
	// The Stack Pointers that are used to load the stack when an entry in the
	// Interrupt Stack Table has an IST value other than 0
	//
	union {
		struct {
			uint64_t Ist1;
			uint64_t Ist2;
			uint64_t Ist3;
			uint64_t Ist4;
			uint64_t Ist5;
			uint64_t Ist6;
			uint64_t Ist7;
		};
		uint64_t Ist[ X64_TASK_STATE_SEGMENT_NUMBER_OF_ISTS ];
	};

	uint32_t Reserved5C;
	uint32_t Reserved60;
	uint16_t Reserved64;

	// I/O Map base Address Field
	// It contains a 16-bit offset from the base of the TSS to the
	// I/O Permission Bit Map
	//
	uint16_t Iopb;
};
#pragma pack( pop )

static_assert( sizeof( _X64_TASK_STATE_SEGMENT ) == 0x68 );

typedef struct _X64_TASK_STATE_SEGMENT X64_TASK_STATE_SEGMENT;
typedef struct _X64_TASK_STATE_SEGMENT* PX64_TASK_STATE_SEGMENT;

#define X64_TASK_STATE_SEGMENT_IOPB_NONE 0

constexpr uint16_t TssComputeIopbOffset( uint16_t offset ) {
	return offset != X64_TASK_STATE_SEGMENT_IOPB_NONE ? offset : sizeof( X64_TASK_STATE_SEGMENT );
}

/**
 * @brief A structure representing a virtual processor
 *
 * This structure represent a virtual processor, its index (number),
 * the exit context upon a virtual processor exit and its registers state
 */
typedef struct _WHSE_VIRTUAL_PROCESSOR {
	uint32_t Index;
	WHSE_PROCESSOR_MODE Mode;
	WHSE_PROCESSOR_VENDOR Vendor;
	PX64_TASK_STATE_SEGMENT Tss;
	WHSE_VP_EXIT_CONTEXT ExitContext;
	WHSE_REGISTERS Registers; // Must be last as it is an array
} WHSE_VIRTUAL_PROCESSOR, * PWHSE_VIRTUAL_PROCESSOR;

enum _MEMORY_BLOCK_TYPE {
	MemoryBlockPhysical,
	MemoryBlockVirtual,
	MemoryBlockPte,

	NumberOfMemoryBlockType
};

typedef enum _MEMORY_BLOCK_TYPE MEMORY_BLOCK_TYPE;

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
typedef struct _WHSE_ALLOCATION_NODE : DLIST_ENTRY {
	// The type of block
	// A block type of MemoryBlockPhysical represent guest physical memory (a Gpa backed by an Hva)
	// A block type of MemoryBlockVirtual represent guest virtual memory (a Gva backed by a Gpa backed by an Hva)
	//
	MEMORY_BLOCK_TYPE BlockType;

	// The Host Virtual Address (HVA) backing the Guest Physical Address (GPA)
	//
	uintptr_t HostVirtualAddress;

	// The Guest Physical Address (GPA)
	//
	uintptr_t GuestPhysicalAddress;

	// The Guest Virtual Address (GVA)
	//
	uintptr_t GuestVirtualAddress;

	// The size of the allocation
	//
	size_t Size;
} WHSE_ALLOCATION_NODE, * PWHSE_ALLOCATION_NODE;

/**
 * @brief A structure to represent the boundaries of address space
 */
struct _ADDRESS_SPACE {
	uintptr_t LowestAddress;
	uintptr_t HighestAddress;
	size_t Size;
};

typedef struct _ADDRESS_SPACE ADDRESS_SPACE;
typedef struct _ADDRESS_SPACE* PADDRESS_SPACE;

/**
 * @brief A structure to represent the boundaries of virtual address space
 */
struct _VIRTUAL_ADDRESS_SPACE {
	ADDRESS_SPACE UserSpace;
	ADDRESS_SPACE SystemSpace;
};

typedef struct _VIRTUAL_ADDRESS_SPACE VIRTUAL_ADDRESS_SPACE;
typedef struct _VIRTUAL_ADDRESS_SPACE* PVIRTUAL_ADDRESS_SPACE;

/**
 * @brief A structure maintaining the necessary data to manage memory allocation
 */
struct _MEMORY_ARENA {
	ADDRESS_SPACE PhysicalAddressSpace;
	VIRTUAL_ADDRESS_SPACE VirtualAddressSpace;
	DLIST_HEADER AllocatedMemoryBlocks;
};

typedef struct _MEMORY_ARENA MEMORY_ARENA;
typedef struct _MEMORY_ARENA* PMEMORY_ARENA;

/**
 * @brief A structure to store the memory layout
 *
 * The structure holds the physical memory and virtual memory boundaries.
 * The structure holds a list of host memory allocations backing the physical guest memory.
 * Paging directories guest physical address and host address is available throught <Pml4PhysicalAddress>
 * and <Pml4HostVa> properties.
 */
typedef struct _WHSE_MEMORY_LAYOUT_DESC {
	MEMORY_ARENA MemoryArena;
	uintptr_t Pml4PhysicalAddress;
	uintptr_t Pml4HostVa;
	uintptr_t InterruptDescriptorTableVirtualAddress;
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

	PVOID Callbacks[ NumberOfCallbacks ];
} WHSE_EXIT_CALLBACKS, * PWHSE_EXIT_CALLBACKS;

static_assert( sizeof( WHSE_EXIT_CALLBACKS::u ) == sizeof( uintptr_t ) * ( WHSE_EXIT_CALLBACK_SLOT::NumberOfCallbacks ), "Wrong WHSE_EXIT_CALLBACKS::u size" );
static_assert( sizeof( WHSE_EXIT_CALLBACKS::Callbacks ) == sizeof( uintptr_t ) * ( WHSE_EXIT_CALLBACK_SLOT::NumberOfCallbacks ), "WHSE_EXIT_CALLBACKS::Callbacks size" );
static_assert( sizeof( WHSE_EXIT_CALLBACKS::Callbacks ) == sizeof( WHSE_EXIT_CALLBACKS::u ), "Wrong WHSE_EXIT_CALLBACKS::Callbacks or WHSE_EXIT_CALLBACKS::u size" );

/**
 * @brief Enumeration to represent the virtual processor ISR callbacks
 */
enum WHSE_ISR_CALLBACK_SLOT : uint16_t {
	DivideByZero,
	Debug,
	NonMaskableInterrupt,
	Breakpoint,
	Overflow,
	BoundRangeExceeded,
	InvalidOpcode,
	DeviceNotAvailable,
	DoubleFault,
	LEGACY_CoprocessorSegmentOverrun,
	InvalidTSS,
	SegmentNotPresent,
	StackSegmentFault,
	GeneralProtectionFault,
	PageFault,
	RESERVED_Isr015,
	x87FloatingPointException,
	AlignmentCheck,
	MachineCheck,
	SimdFloatingPointException,
	VirtualiuzationException,
	ControlProtectionException,
	RESERVED_Isr022,
	RESERVED_Isr023,
	RESERVED_Isr024,
	RESERVED_Isr025,
	RESERVED_Isr026,
	RESERVED_Isr027,
	HypervisorInjectionException,
	VmmCommunicationException,
	SecurityException,
	RESERVED_Isr031,
	Isr032, Isr033, Isr034, Isr035, Isr036, Isr037, Isr038, Isr039, Isr040, Isr041, Isr042, Isr043, Isr044, Isr045, Isr046, Isr047,
	Isr048, Isr049, Isr050, Isr051, Isr052, Isr053, Isr054, Isr055, Isr056, Isr057, Isr058, Isr059, Isr060, Isr061, Isr062, Isr063,
	Isr064, Isr065, Isr066, Isr067, Isr068, Isr069, Isr070, Isr071, Isr072, Isr073, Isr074, Isr075, Isr076, Isr077, Isr078, Isr079,
	Isr080, Isr081, Isr082, Isr083, Isr084, Isr085, Isr086, Isr087, Isr088, Isr089, Isr090, Isr091, Isr092, Isr093, Isr094, Isr095,
	Isr096, Isr097, Isr098, Isr099, Isr100, Isr101, Isr102, Isr103, Isr104, Isr105, Isr106, Isr107, Isr108, Isr109, Isr110, Isr111,
	Isr112, Isr113, Isr114, Isr115, Isr116, Isr117, Isr118, Isr119, Isr120, Isr121, Isr122, Isr123, Isr124, Isr125, Isr126, Isr127,
	Isr128, Isr129, Isr130, Isr131, Isr132, Isr133, Isr134, Isr135, Isr136, Isr137, Isr138, Isr139, Isr140, Isr141, Isr142, Isr143,
	Isr144, Isr145, Isr146, Isr147, Isr148, Isr149, Isr150, Isr151, Isr152, Isr153, Isr154, Isr155, Isr156, Isr157, Isr158, Isr159,
	Isr160, Isr161, Isr162, Isr163, Isr164, Isr165, Isr166, Isr167, Isr168, Isr169, Isr170, Isr171, Isr172, Isr173, Isr174, Isr175,
	Isr176, Isr177, Isr178, Isr179, Isr180, Isr181, Isr182, Isr183, Isr184, Isr185, Isr186, Isr187, Isr188, Isr189, Isr190, Isr191,
	Isr192, Isr193, Isr194, Isr195, Isr196, Isr197, Isr198, Isr199, Isr200, Isr201, Isr202, Isr203, Isr204, Isr205, Isr206, Isr207,
	Isr208, Isr209, Isr210, Isr211, Isr212, Isr213, Isr214, Isr215, Isr216, Isr217, Isr218, Isr219, Isr220, Isr221, Isr222, Isr223,
	Isr224, Isr225, Isr226, Isr227, Isr228, Isr229, Isr230, Isr231, Isr232, Isr233, Isr234, Isr235, Isr236, Isr237, Isr238, Isr239,
	Isr240, Isr241, Isr242, Isr243, Isr244, Isr245, Isr246, Isr247, Isr248, Isr249, Isr250, Isr251, Isr252, Isr253, Isr254, Isr255,

	NumberOfInterruptRoutines
};

constexpr bool IsrHasErrorCode( uint8_t index ) {
	bool ret = false;

	switch ( index ) {
		case DoubleFault:
		case InvalidTSS:
		case SegmentNotPresent:
		case StackSegmentFault:
		case GeneralProtectionFault:
		case PageFault:
		case AlignmentCheck:
		case ControlProtectionException:
		case VmmCommunicationException:
		case SecurityException:
			ret = true;
	}

	return ret;
}

/*
 * @brief A structure to hold the frame of an interrupt
 */
struct _X64_INTERRUPT_FRAME {
	uint64_t Rip;
	uint64_t Cs;
	uint64_t Rflags;
	uint64_t Rsp; // Original rsp
	uint64_t Ss;
};

typedef struct _X64_INTERRUPT_FRAME X64_INTERRUPT_FRAME;
typedef struct _X64_INTERRUPT_FRAME* PX64_INTERRUPT_FRAME;

typedef WHSE_CALLBACK_RETURNTYPE( WHSECALLBACKAPI WHSE_ISR_CALLBACK )( _WHSE_PARTITION* Partition, PX64_INTERRUPT_FRAME Frame, uint32_t ErrorCode );

/**
 * @brief A structure to hold pointers to the virtual processor ISR callbacks
 */
typedef union _WHSE_ISR_CALLBACKS {
	struct {
		WHSE_ISR_CALLBACK DivideByZeroCallback;
		WHSE_ISR_CALLBACK DebugCallback;
		WHSE_ISR_CALLBACK NonMaskableInterruptCallback;
		WHSE_ISR_CALLBACK BreakpointCallback;
		WHSE_ISR_CALLBACK OverflowCallback;
		WHSE_ISR_CALLBACK BoundRangeExceededCallback;
		WHSE_ISR_CALLBACK InvalidOpcodeCallback;
		WHSE_ISR_CALLBACK DeviceNotAvailableCallback;
		WHSE_ISR_CALLBACK DoubleFaultCallback;
		WHSE_ISR_CALLBACK LEGACY_CoprocessorSegmentOverrunCallback;
		WHSE_ISR_CALLBACK InvalidTSSCallback;
		WHSE_ISR_CALLBACK SegmentNotPresentCallback;
		WHSE_ISR_CALLBACK StackSegmentFault;
		WHSE_ISR_CALLBACK GeneralProtectionFaultCallback;
		WHSE_ISR_CALLBACK PageFaultCallback;
		WHSE_ISR_CALLBACK RESERVED_015;
		WHSE_ISR_CALLBACK x87FloatingPointExceptionCallback;
		WHSE_ISR_CALLBACK AlignmentCheckCallback;
		WHSE_ISR_CALLBACK MachineCheckCallback;
		WHSE_ISR_CALLBACK SimdFloatingPointExceptionCallback;
		WHSE_ISR_CALLBACK VirtualizationExceptionCallback;
		WHSE_ISR_CALLBACK ControlProtectionExceptionCallback;
		WHSE_ISR_CALLBACK RESERVED_022;
		WHSE_ISR_CALLBACK RESERVED_023;
		WHSE_ISR_CALLBACK RESERVED_024;
		WHSE_ISR_CALLBACK RESERVED_025;
		WHSE_ISR_CALLBACK RESERVED_026;
		WHSE_ISR_CALLBACK RESERVED_027;
		WHSE_ISR_CALLBACK HypervisorInjectionExceptionCallback;
		WHSE_ISR_CALLBACK VmmCommunicationExceptionCallback;
		WHSE_ISR_CALLBACK SecurityExceptionCallback;
		WHSE_ISR_CALLBACK RESERVED_31;
	} u;

	WHSE_ISR_CALLBACK Callbacks[ NumberOfInterruptRoutines ];
} WHSE_ISR_CALLBACKS, * PWHSE_ISR_CALLBACKS;

static_assert( sizeof( WHSE_ISR_CALLBACKS::u ) == sizeof( uintptr_t ) * ( WHSE_ISR_CALLBACK_SLOT::Isr032 ), "Wrong WHSE_ISR_CALLBACKS::u size" );
static_assert( sizeof( WHSE_ISR_CALLBACKS::Callbacks ) == sizeof( uintptr_t ) * ( WHSE_ISR_CALLBACK_SLOT::NumberOfInterruptRoutines ), "WHSE_ISR_CALLBACKS::Callbacks size" );

/**
 * @brief A structure to represent a partition
 *
 * A partition is the highest level structure and holds everything needed
 * to run code on a virtual processor.
 * The <Handle> property is an opaque pointer to a <WHV_PARTITION> structure.
 * The <MemoryLayout> holds the properties of the memory layout.
 * The <VirtualProcessor> holds the properties of a virtual processor.
 * The <ExitCallbacks> hold an array of pointers to the virtual processor exits callbacks.
 * The <IsrCallbacks> hold an array of pointers to the virtual processor interrupt routines callbacks.
 */
typedef struct _WHSE_PARTITION {
	WHSE_PARTITION_HANDLE Handle;
	WHSE_MEMORY_LAYOUT_DESC MemoryLayout;
	WHSE_VIRTUAL_PROCESSOR VirtualProcessor;
	WHSE_EXIT_CALLBACKS ExitCallbacks;
	WHSE_ISR_CALLBACKS IsrCallbacks;
} WHSE_PARTITION, * PWHSE_PARTITION;

#endif // !WINHVDEFS_HPP