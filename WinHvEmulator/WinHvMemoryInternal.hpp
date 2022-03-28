#ifndef WINHVMEMORYINTERNAL_HPP
#define WINHVMEMORYINTERNAL_HPP

#include <windows.h>
#include <winhvplatform.h>

#include "WinHvPartition.hpp"

#include <cstdint>

#pragma pack( push, 1 )
typedef struct _MMPTE_HARDWARE
{
	union
	{
		struct
		{
			UINT64 Valid : 1;
			UINT64 Write : 1;
			UINT64 Owner : 1;
			UINT64 WriteThrough : 1;
			UINT64 CacheDisable : 1;
			UINT64 Accessed : 1;
			UINT64 Dirty : 1;
			UINT64 LargePage : 1;
			UINT64 Available : 4;
			UINT64 PageFrameNumber : 36;
			UINT64 ReservedForHardware : 4;
			UINT64 ReservedForSoftware : 11;
			UINT64 NoExecute : 1;
		};
		UINT64 AsUlonglong;
	};
} MMPTE_HARDWARE, * PMMPTE_HARDWARE;
#pragma pack( pop )

static_assert( sizeof( MMPTE_HARDWARE ) == 8 );

#pragma pack( push, 1 )
typedef struct _IDT_ENTRY {
	uint16_t Low;
	uint16_t Selector;
	uint8_t InterruptStackTable;
	uint8_t Attributes;
	uint16_t Mid;
	uint32_t High;
	uint32_t Reserved;
} IDT_ENTRY, * PIDT_ENTRY;
#pragma pack( pop )

static_assert( sizeof( IDT_ENTRY ) == 16 );

constexpr static size_t NUMBER_OF_IDT_DESCRIPTORS = 256;

#pragma pack( push, 1 )
typedef struct _GDT_ENTRY {
	uint16_t LimitLow;
	uint16_t BaseLow;
	uint8_t BaseMid;
	uint8_t Access;
	uint8_t LimitHigh : 4;
	uint8_t Flags : 4;
	uint8_t BaseHigh;
} GDT_ENTRY, * PGDT_ENTRY;
#pragma pack( pop )

static_assert( sizeof( GDT_ENTRY ) == 8 );

constexpr static size_t NUMBER_OF_GDT_DESCRIPTORS = 5;

// Decompose a virtual address into paging indexes
//
HRESULT WhSiDecomposeVirtualAddress( uintptr_t VirtualAddress, uint16_t* Pml4Index, uint16_t* PdpIndex, uint16_t* PdIndex, uint16_t* PtIndex, uint16_t* Offset );

// Get one or more physical page
//
HRESULT WhSiGetNextPhysicalPages( WHSE_PARTITION* Partition, size_t NumberOfPages, uintptr_t* PhysicalPageAddress );

// Get one physical page
//
HRESULT WhSiGetNextPhysicalPage( WHSE_PARTITION* Partition, uintptr_t* PhysicalPageAddress );

// Internal helper to allocate host memory to guest physical memory
//
HRESULT WhSiAllocateGuestPhysicalMemory( WHSE_PARTITION* Partition, PVOID* HostVa, uintptr_t* GuestPa, size_t* Size, WHSE_MEMORY_ACCESS_FLAGS Flags );

// Internal function to setup paging
//
HRESULT WhSiSetupPaging( WHSE_PARTITION* Partition, uintptr_t* Pml4PhysicalAddress );

// Internal function to insert page table in the paging directory
// Allocate PML4 entry, PDP entry, PD entry and PT entry
//
HRESULT WhSiInsertPageTableEntry( WHSE_PARTITION* Partition, uintptr_t VirtualAddress );

// Find a suitable Guest VA
//
HRESULT WhSiFindBestGVA( WHSE_PARTITION* Partition, uintptr_t* GuestVa, size_t Size );

// Setup GDT
//
HRESULT WhSiSetupGlobalDescriptorTable( WHSE_PARTITION* Partition, WHSE_REGISTERS Registers );

// Setup IDT
//
HRESULT WhSiSetupInterruptDescriptorTable( WHSE_PARTITION* Partition, WHSE_REGISTERS Registers );

#endif // !WINHVMEMORYINTERNAL_HPP