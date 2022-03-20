#ifndef EXECUTOR_HPP
#define EXECUTOR_HPP

#include <windows.h>

#include <cstdint>

#include "../WinHvEmulator/WinHvEmulator.hpp"

typedef struct _RUN_PARAMS {
	uintptr_t Entrypoint;
	uintptr_t Stack;
	WHSE_PARTITION* Partition;
} RUN_PARAMS, * PRUN_PARAMS;

typedef struct _RUN_OPTIONS {
	uint8_t* Code;
	size_t CodeSize;
	uintptr_t BaseAddress;
	PROCESSOR_MODE Mode;
} RUN_OPTIONS, * PRUN_OPTIONS;

// Execute a shellcode through a virtual processor
//
DWORD WINAPI Run( const RUN_OPTIONS& options );

#endif // !EXECUTOR_HPP