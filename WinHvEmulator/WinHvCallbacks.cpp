#include "WinHvCallbacks.hpp"
#include "winerror.h"

/**
 * @brief Register a callback for the given vm exit
 *
 * @param Partition The VM partition
 * @param Slot The callback slot to register the callback to
 * @param Callback The callback to register
 * @return A result code
 */
HRESULT WhSeRegisterExitCallback( WHSE_PARTITION* Partition, WHSE_EXIT_CALLBACK_SLOT Slot, WHSE_CALLBACK Callback ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( static_cast< int >( Slot ) < 0 || static_cast< int >( Slot ) >= WHSE_EXIT_CALLBACK_SLOT::NumberOfCallbacks )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( Callback == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	Partition->ExitCallbacks.Callbacks[ static_cast< int >( Slot ) ] = Callback;

	return S_OK;
}

/**
 * @brief Get the registered callback for the given vm exit
 *
 * @param Partition The VM partition
 * @param Slot The callback slot to get
 * @param Callback The pointer to the returned callback
 * @return A result code
 */
HRESULT WhSeGetExitCallback( WHSE_PARTITION* Partition, WHSE_EXIT_CALLBACK_SLOT Slot, WHSE_CALLBACK* Callback ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( static_cast< int >( Slot ) < 0 || static_cast< int >( Slot ) >= WHSE_EXIT_CALLBACK_SLOT::NumberOfCallbacks )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( Callback == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	*Callback = Partition->ExitCallbacks.Callbacks[ Slot ];

	return S_OK;
}

/**
 * @brief Unegister a callback for the given vm exit
 *
 * @param Partition The VM partition
 * @param Slot The callback slot to unregister
 * @return A result code
 */
HRESULT WhSeUnregisterExitCallback( WHSE_PARTITION* Partition, WHSE_EXIT_CALLBACK_SLOT Slot ) {
	if ( Partition == nullptr )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	if ( static_cast< int >( Slot ) < 0 || static_cast< int >( Slot ) >= WHSE_EXIT_CALLBACK_SLOT::NumberOfCallbacks )
		return HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );

	Partition->ExitCallbacks.Callbacks[ Slot ] = nullptr;

	return S_OK;
}