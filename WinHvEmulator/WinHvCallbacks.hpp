#ifndef WINHVCALLBACKS_HPP
#define WINHVCALLBACKS_HPP

#include "WinHvDefs.hpp"
#include "WinHvExports.hpp"

#include <windows.h>

// Register a callback for the given vm exit
//
HRESULT WhSeRegisterExitCallback( WHSE_PARTITION* Partition, WHSE_EXIT_CALLBACK_SLOT Slot, WHSE_CALLBACK Callback );

// Get the registered callback for the given vm exit
//
HRESULT WhSeGetExitCallback( WHSE_PARTITION* Partition, WHSE_EXIT_CALLBACK_SLOT Slot, WHSE_CALLBACK* Callback );

// Unegister a callback for the given vm exit
//
HRESULT WhSeUnregisterExitCallback( WHSE_PARTITION* Partition, WHSE_EXIT_CALLBACK_SLOT Slot );

#endif // !WINHVCALLBACKS_HPP