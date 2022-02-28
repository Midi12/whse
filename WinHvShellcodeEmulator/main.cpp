#include <windows.h>

#include "../WinHvEmulator/WinHvEmulator.hpp"
#include "../optlist/optlist.h"

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

void Usage() {
	std::cout << "- i <filename> binary file containing the shellcode to execute" << std::endl;
	std::cout << "- m[kernel | user] kernel or user mode code" << std::endl;
	std::cout << "- b <base> load the shellcode at a specified virtual address" << std::endl;
}

// 0:  48 c7 c0 37 13 00 00    mov    rax, 0x1337
uint8_t testcode[] = { 0x48, 0xC7, 0xC0, 0x37, 0x13, 0x00, 0x00 };

int main( int argc, char* const argv[], char* const envp[] ) {
	if ( !WhSeIsHypervisorPresent() )
		EXIT_WITH_MESSAGE( "Hypervisor not present" );

	// Parse command line
	//
	// -i <filename> binary file containing the shellcode to execute
	// -m [kernel|user] kernel or user mode code
	// -b <base> load the shellcode at a specified virtual address

	option_t* options = nullptr;
	options = GetOptList( argc, argv, "i:m:b:?" );
	
	while ( options != nullptr ) {
		option_t* option = options;
		options = options->next;

		if ( option->option == '?' ) {
			Usage();
			break;
		}
	}

	FreeOptList( options );


	// Execute shellcode on a virtualized processor
	//
	Execute( testcode, sizeof( decltype( testcode ) ) );

	return EXIT_SUCCESS;
}