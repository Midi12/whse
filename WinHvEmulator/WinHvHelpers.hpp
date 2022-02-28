#ifndef WINHVHELPERS_HPP
#define WINHVHELPERS_HPP

#include "WinHvExports.hpp"

#include <windows.h>
#include <cstdint>

//	Check if the hypervisor is available
//	Must be called prior to any other calls
//	To check for hypervisor presence
//
bool WHSEAPI WhSeIsHypervisorPresent();

// Wrapper around GetLastError
//
uint32_t WHSEAPI WhSeGetLastError();

// Wrapper around GetLastError and HRESULT_FROM_WIN32
//
HRESULT WHSEAPI WhSeGetLastHresult();

#endif // !WINHVHELPERS_HPP