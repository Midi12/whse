#ifndef WINHVPARTITION_HPP
#define WINHVPARTITION_HPP

#include <windows.h>
#include <winhvplatform.h>

#include "WinHvExports.hpp"
#include "WinHvDefs.hpp"

// Create an hypervisor partition
//
HRESULT WHSEAPI WhSeCreatePartition( WHSE_PARTITION** pPartition );

// Delete an hypervisor partition
//
HRESULT WHSEAPI WhSeDeletePartition( WHSE_PARTITION** Partition );

#endif // !WINHVPARTITION_HPP