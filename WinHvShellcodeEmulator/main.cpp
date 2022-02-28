#include <windows.h>

#include "../WinHvEmulator/WinHvEmulator.hpp"

#include "Executor.hpp"

#include <cstdint>
#include <iostream>

#define EXIT_WITH_MESSAGE( x ) \
	{ \
		std::cerr << x << std::endl; \
		std::cerr << "Last hresult = " << std::hex << ::WhSeGetLastHresult() << std::endl; \
		std::cerr << "Last error code = " << std::hex << ::WhSeGetLastError() << std::endl; \
		return EXIT_FAILURE; \
	} \

// 0:  48 c7 c0 37 13 00 00    mov    rax, 0x1337
uint8_t testcode[] = { 0x48, 0xC7, 0xC0, 0x37, 0x13, 0x00, 0x00 };

int wmain( int argc, wchar_t* argv[], wchar_t* envp[] ) {
	if ( !::WhSeIsHypervisorPresent() )
		EXIT_WITH_MESSAGE( "Hypervisor not present" );

	// Parse command line
	//
	// todo

	// Execute shellcode on a virtualized processor
	//
	Execute( testcode, sizeof( decltype( testcode ) ) );

	return EXIT_SUCCESS;
}