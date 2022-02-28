#ifndef WINHVPROCESSOR_HPP
#define WINHVPROCESSOR_HPP

#include <windows.h>
#include <winhvplatform.h>

#include "WinHvExports.hpp"
#include "WinHvDefs.hpp"

// Create a virtual processor in a partition
//
HRESULT WHSEAPI WhSeCreateProcessor( WHSE_PARTITION* Partition );

// Set a virtual processor registers
//
HRESULT WHSEAPI WhSeSetProcessorRegisters( WHSE_PARTITION* Partition, WHSE_REGISTERS Registers );

// Get a virtual processor registers
//
HRESULT WHSEAPI WhSeGetProcessorRegisters( WHSE_PARTITION* Partition, WHSE_REGISTERS Registers );

// Run a virtual processor registers
//
HRESULT WHSEAPI WhSeRunProcessor( WHSE_PARTITION* Partition, WHSE_VP_EXIT_REASON * ExitReason );

// Delete a virtual processor from the partition
//
HRESULT WHSEAPI WhSeDeleteProcessor( WHSE_PARTITION* Partition );

#endif // !WINHVPROCESSOR_HPP
