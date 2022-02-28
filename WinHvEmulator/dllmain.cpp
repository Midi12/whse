#include <windows.h>

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