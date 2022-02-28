#include "WinHvEmulator.hpp"

#include <winhvplatform.h>

//	Check if the hypervisor is available
//	Must be called prior to any other calls
//	To check for hypervisor presence
//
bool WhSeIsHypervisorPresent() {
	auto capabilities = WHV_CAPABILITY { 0 };
	uint32_t written = 0;

	// Check if hypervisor is present
	//
	auto hresult = ::WHvGetCapability( WHvCapabilityCodeHypervisorPresent, &capabilities, sizeof( decltype( capabilities ) ), &written );
	if ( FAILED( hresult ) || !capabilities.HypervisorPresent ) {
		return false;
	}

	return true;
}

// Wrapper around GetLastError
//
uint32_t WhSeGetLastError() {
	return ::GetLastError();
}

// Wrapper around GetLastError and HRESULT_FROM_WIN32
//
HRESULT WhSeGetLastHresult() {
	auto lasterror = WhSeGetLastError();
	return HRESULT_FROM_WIN32( lasterror );
}