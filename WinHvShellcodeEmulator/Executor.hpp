#ifndef EXECUTOR_HPP
#define EXECUTOR_HPP

#include <windows.h>

#include <cstdint>

#include "../WinHvEmulator/WinHvEmulator.hpp"

enum class PROCESSOR_MODE : uint8_t {
	None,
	KernelMode,
	UserMode
};

typedef struct EXECUTOR_PARAMS {
	uintptr_t Entrypoint;
	uintptr_t Stack;
	WHSE_PARTITION* Partition;
	PROCESSOR_MODE Mode;
};

// Execute a shellcode through a virtual processor
//
DWORD WINAPI Execute( const uint8_t* Shellcode, const size_t Size );

#endif // !EXECUTOR_HPP