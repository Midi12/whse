#include <windows.h>
#include <shlwapi.h>

#include "../WinHvEmulator/WinHvEmulator.hpp"
#include "../optlist/optlist.h"

#include "Executor.hpp"

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cstdio>

#define EXIT_WITH_MESSAGE( x ) \
	{ \
		printf( x "\n" ); \
		printf( "Last hresult = %llx\n", static_cast< unsigned long long >( WhSeGetLastHresult() ) ); \
		printf( "Last error code = %llx\n", static_cast< unsigned long long >( WhSeGetLastError() ) ); \
		return EXIT_FAILURE; \
	} \

void Usage() {
	printf( "-i <filename> binary file containing the shellcode to execute\n" );
	printf( "-m <kernel | user> kernel or user mode code\n" );
	printf( "-b <base> load the shellcode at a specified virtual address\n" );
}

// Read input file
//
bool ReadInputFile( const char* Filename, uint8_t** Code, size_t* CodeSize ) {
	if ( Code == nullptr ) {
		return false;
	}

	// Open the input file
	//
	FILE* fp = nullptr;
	if ( fopen_s( &fp, Filename, "rb" ) != 0 )
		return false;

	if ( fp == nullptr )
		return false;

	// Get the file size
	//
	fseek( fp, 0, SEEK_END );
	*CodeSize = ftell( fp );
	*Code = reinterpret_cast< uint8_t* >( malloc( *CodeSize ) );
	if ( *Code == nullptr ) {
		fclose( fp );
		return false;
	}

	fseek( fp, 0, SEEK_SET );

	fread( *Code, *CodeSize, 1, fp );

	fclose( fp );

	return true;
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
			size_t relativePathSize = strlen( option_->argument ) + 2 + 1;
			char* relativePath = reinterpret_cast< char* >( malloc( relativePathSize ) );
			if ( relativePath == nullptr )
				return false;

			memset( relativePath, 0, relativePathSize );


			strcpy_s( relativePath, 2 + 1, "./" );
			strcat_s( relativePath, relativePathSize, option_->argument );
			
			if ( !ReadInputFile( relativePath, &options.Code, &options.CodeSize ) ) {
				free( relativePath );
				FreeOptList( options_ );

				return false;
			}

			free( relativePath );
		}
		else if ( option_->option == 'm' ) {
			if ( !strcmp( option_->argument, "user" ) ) {
				options.Mode = PROCESSOR_MODE::UserMode;
			}
			else if ( !strcmp( option_->argument, "kernel" ) ) {
				options.Mode = PROCESSOR_MODE::KernelMode;
			}
		}
		else if ( option_->option == 'b' ) {
			options.BaseAddress =
				*reinterpret_cast< uint16_t* >( option_->argument ) == static_cast< uint16_t >( 0x3078 ) // '0x'
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

#if defined( _DEBUG )
	// Fix current working directory
	//
	char dir[ MAX_PATH ] = { 0 };
	strcpy_s( dir, sizeof( decltype( dir ) ), argv[ 0 ] );
	PathRemoveFileSpec( dir );
	SetCurrentDirectory( dir );
#endif

	EXECUTOR_OPTIONS options { };
	if ( !ParseCommandLine( argc, argv, options ) )
		return EXIT_SUCCESS;

	if ( options.Code == nullptr || options.CodeSize == 0 )
		EXIT_WITH_MESSAGE( "No shellcode data" );

	if ( options.Mode == PROCESSOR_MODE::None ) {
		printf( "No processor mode specified, defaulting to 'UserMode'\n" );
		options.Mode = PROCESSOR_MODE::UserMode;
	}

	// Execute shellcode on a virtualized processor
	//
	Execute( options );

	free( options.Code );

	return EXIT_SUCCESS;
}