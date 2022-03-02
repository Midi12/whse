#include <windows.h>

#include "../WinHvEmulator/WinHvEmulator.hpp"
#include "../optlist/optlist.h"

#include "Executor.hpp"

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cstdio>

#define EXIT_WITH_MESSAGE( x ) \
	{ \
		printf( x ); \
		printf( "Last hresult = %llx", static_cast< unsigned long long >( WhSeGetLastHresult() ) ); \
		printf( "Last error code = %llx", static_cast< unsigned long long >( WhSeGetLastError() ) ); \
		return EXIT_FAILURE; \
	} \

void Usage() {
	printf( "-i <filename> binary file containing the shellcode to execute" );
	printf( "-m <kernel | user> kernel or user mode code" );
	printf( "-b <base> load the shellcode at a specified virtual address" );
}

// Read input file
//
void ReadInputFile( const char* Filename, uint8_t** Code, size_t* CodeSize ) {
	// Open the input file
	//
	FILE* fp = fopen( Filename, "rb" );
	if ( fp == nullptr )
		return;

	// Get the file size
	//
	fseek( fp, 0, SEEK_END );
	*CodeSize = ftell( fp );
	*Code = reinterpret_cast< uint8_t * >( malloc( *CodeSize ) );

	fseek( fp, 0, SEEK_SET );

	constexpr uint32_t bufferSize = 4096;
	uint8_t buffer[ bufferSize ] = { 0 };

	size_t size = *CodeSize;
	while ( size - bufferSize > bufferSize) {
		fread( buffer, bufferSize, 1, fp );
		size -= bufferSize;
		memcpy( *Code, buffer, bufferSize );
	}

	fread( buffer, size, 1, fp );
	memcpy( *Code, buffer, size );

	fclose( fp );
}

// Parse command line :
// -i <filename> binary file containing the shellcode to execute
// -m [kernel|user] kernel or user mode code
// -b <base> load the shellcode at a specified virtual address
//
bool ParseCommandLine( int argc, char* const argv[], EXECUTOR_OPTIONS& options ) {
	option_t* options_ = nullptr;
	options_ = GetOptList( argc, argv, "i:m:b:?" );
	
	if ( options_->option == '?' ) {
		Usage();
		return false;
	}

	while ( options_ != nullptr ) {
		option_t* option_ = options_;
		options_ = options_->next;

		if ( option_->option == 'i' ) {
			ReadInputFile( option_->argument, &options.Code, &options.CodeSize );
		} else if ( option_->option == 'm' ) {
			if ( !strcmp( option_->argument, "user" ) ) {
				options.Mode = PROCESSOR_MODE::UserMode;
			} else if ( !strcmp( option_->argument, "kernel" ) ) {
				options.Mode = PROCESSOR_MODE::KernelMode;
			}
		} else if ( option_->option == 'b' ) {
			options.BaseAddress =
				*reinterpret_cast< uint16_t* >( option_->argument ) == static_cast< uint16_t >(0x3078) // '0x'
				? strtol( option_->argument, nullptr, 0 )
				: strtol( option_->argument, nullptr, 16 );
		}
	}

	FreeOptList( options_ );
	return true;
}

int main( int argc, char* const argv[], char* const envp[] ) {
	if ( !WhSeIsHypervisorPresent() )
		EXIT_WITH_MESSAGE( "Hypervisor not present" );

	EXECUTOR_OPTIONS options { };
	if ( !ParseCommandLine( argc, argv, options ))
		return EXIT_SUCCESS;

	if ( options.Code == nullptr || options.CodeSize == 0 )
		EXIT_WITH_MESSAGE( "No shellcode data" );

	if ( options.Mode == PROCESSOR_MODE::None ) {
		printf( "No processor mode specified, defaulting to 'UserMode'" );
		options.Mode = PROCESSOR_MODE::UserMode;
	}

	// Execute shellcode on a virtualized processor
	//
	Execute( options );

	free( options.Code );

	return EXIT_SUCCESS;
}