#include "WinHvCallbacks.hpp"
#include "winerror.h"

// Register a callback for the given vm exit
//
HRESULT WhSeRegisterExitCallback( WHSE_PARTITION* Partition, WHSE_EXIT_CALLBACK_SLOT Slot, WHSE_CALLBACK Callback ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( static_cast< int >( Slot ) < 0 || static_cast< int >( Slot ) >= WHSE_EXIT_CALLBACK_SLOT::NumberOfCallbacks )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( Callback == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	Partition->ExitCallbacks[ Slot ] = Callback;

	return S_OK;
}

// Get the registered callback for the given vm exit
//
HRESULT WhSeGetExitCallback( WHSE_PARTITION* Partition, WHSE_EXIT_CALLBACK_SLOT Slot, WHSE_CALLBACK* Callback ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( static_cast< int >( Slot ) < 0 || static_cast< int >( Slot ) >= WHSE_EXIT_CALLBACK_SLOT::NumberOfCallbacks )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( Callback == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	*Callback = Partition->ExitCallbacks[ Slot ];

	return S_OK;
}

// Unegister a callback for the given vm exit
//
HRESULT WhSeUnregisterExitCallback( WHSE_PARTITION* Partition, WHSE_EXIT_CALLBACK_SLOT Slot ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( static_cast< int >( Slot ) < 0 || static_cast< int >( Slot ) >= WHSE_EXIT_CALLBACK_SLOT::NumberOfCallbacks )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	Partition->ExitCallbacks[ Slot ] = nullptr;

	return S_OK;
}