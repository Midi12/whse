#include "WinHvEmulator.hpp"

#include <winhvplatform.h>

/**
 * @brief Check if the hypervisor is available
 *
 * Check if the hypervisor is available
 * Must be called prior to any other calls
 * To check for hypervisor presence
 *
 * @return A boolean indicating if the hypervisor is present
 */
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

/**
 * @brief Wrapper around GetLastError
 *
 * @return A code indicating the last error
 */
uint32_t WhSeGetLastError() {
	return ::GetLastError();
}

/**
 * @brief Wrapper around GetLastError and HRESULT_FROM_WIN32
 *
 * @return A code indicating the last error
 */
HRESULT WhSeGetLastHresult() {
	auto lasterror = WhSeGetLastError();
	return HRESULT_FROM_WIN32( lasterror );
}

#define ARCH_X64_HIGHESTORDER_BIT_IMPLEMENTED 47
#define ARCH_X64_CANONICAL_BITMASK  ~( ( 1ul << ( ARCH_X64_HIGHESTORDER_BIT_IMPLEMENTED + 1 ) ) - 1 )

/**
 * @brief An helper function to know if a virtual address is canonical
 *
 * @return A boolean indicating if the virtual address <VirtualAddress> is canonical (true) or not (false)
 */
bool WhSeIsCanonicalAddress( uintptr_t VirtualAddress ) {
	bool highest_bit_set = ( ( VirtualAddress & ( 1ul << ARCH_X64_HIGHESTORDER_BIT_IMPLEMENTED ) ) >> ARCH_X64_HIGHESTORDER_BIT_IMPLEMENTED ) == 1;

	uintptr_t masked = VirtualAddress & ARCH_X64_CANONICAL_BITMASK;

	return highest_bit_set ? masked == ARCH_X64_CANONICAL_BITMASK : masked == 0x00000000'00000000;
}