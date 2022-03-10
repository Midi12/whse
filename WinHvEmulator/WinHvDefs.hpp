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

typedef struct WHSE_PARTITION {
	WHSE_PARTITION_HANDLE Handle;
	WHSE_MEMORY_LAYOUT_DESC MemoryLayout;
	WHSE_VIRTUAL_PROCESSOR VirtualProcessor;
};

#endif // !WINHVDEFS_HPP