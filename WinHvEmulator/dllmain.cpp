#include <windows.h>

/** @file
 * @brief This file is the entry point when building as a shared library
 *
 */

BOOLEAN WINAPI DllMain( IN HINSTANCE hInstance, IN DWORD nReason, IN LPVOID Reserved ) {
	BOOLEAN success = TRUE;

	switch ( nReason ) {
	case DLL_PROCESS_ATTACH:
		::DisableThreadLibraryCalls( hInstance );
		break;
	default:
		break;
	}

	return success;
}