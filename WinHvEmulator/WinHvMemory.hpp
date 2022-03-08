#ifndef WINHVMEMORY_HPP
#define WINHVMEMORY_HPP

#include <windows.h>
#include <winhvplatform.h>

#include "WinHvExports.hpp"
#include "WinHvPartition.hpp"

#include <cstdint>

// Allocate memory in guest physical address space (backed by host memory)
//
HRESULT WHSEAPI WhSeAllocateGuestPhysicalMemory( WHSE_PARTITION* Partition, PVOID* HostVa, uintptr_t GuestPa, size_t* Size, WHSE_MEMORY_ACCESS_FLAGS Flags );

// Map memory from host to guest physical address space (backed by host memory)
//
HRESULT WHSEAPI WhSeMapHostToGuestPhysicalMemory( WHSE_PARTITION* Partition, PVOID HostVa, uintptr_t GuestPa, size_t Size, WHSE_MEMORY_ACCESS_FLAGS Flags );

// Allocate memory in guest virtual address space (backed by host memory)
//
HRESULT WHSEAPI WhSeAllocateGuestVirtualMemory( WHSE_PARTITION* Partition, PVOID* HostVa, uintptr_t GuestVa, size_t* Size, WHSE_MEMORY_ACCESS_FLAGS Flags );

// Map memory from host to guest virtual address space (backed by host memory)
//
HRESULT WHSEAPI WhSeMapHostToGuestVirtualMemory( WHSE_PARTITION* Partition, PVOID HostVa, uintptr_t GuestVa, size_t Size, WHSE_MEMORY_ACCESS_FLAGS Flags );


// Free memory in guest address space
//
HRESULT WHSEAPI WhSeFreeGuestMemory( WHSE_PARTITION* Partition, PVOID HostVa, uintptr_t GuestVa, size_t Size );

// Initialize paging and other stuff for the partition
//
HRESULT WHSEAPI WhSeInitializeMemoryLayout( WHSE_PARTITION* Partition );

// Translate guest virtual address to guest physical address
//
HRESULT WHSEAPI WhSeTranslateGvaToGpa( WHSE_PARTITION* Partition, uintptr_t VirtualAddress, uintptr_t* PhysicalAddress, WHV_TRANSLATE_GVA_RESULT* TranslationResult, size_t Size );

// Page fault handler
//
HRESULT WHSEAPI WhSePageFaultHandler( WHSE_PARTITION* Partition );

#endif // !WINHVMEMORY_HPP