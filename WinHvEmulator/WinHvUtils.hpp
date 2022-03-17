#ifndef WINHVUTILS_HPP
#define WINHVUTILS_HPP

#include <cstddef>
#include <cstdint>

/** @file
 * @brief This file expose utility helpers
 *
 */

#pragma warning( push )
#pragma warning( disable: 4455 )
constexpr size_t operator ""KiB( size_t x ) {
	return 1024 * x;
}

constexpr size_t operator ""MiB( size_t x ) {
	return 1KiB * 1024 * x;
}

constexpr size_t operator ""GiB( size_t x ) {
	return 1MiB * 1024 * x;
}

constexpr size_t operator ""TiB( size_t x ) {
	return 1GiB * 1024 * x;
}
#pragma warning( pop )

/**
 * @brief The page size value
 */
constexpr size_t PAGE_SIZE = 4096;

/**
 * @brief Align a pointer up to the next page boundary
 *
 * Align a pointer up to the next page boundary.
 * It will not align up if already aligned to page boundary
 *
 * @param x The value to align up
 * @return The aligned value
 */
constexpr size_t ALIGN_UP( size_t x ) {
	return ( ( PAGE_SIZE - 1 ) & x ) ? ( ( x + PAGE_SIZE ) & ~( PAGE_SIZE - 1 ) ) : x;
}

/**
 * @brief Align a pointer to the page boundary
 *
 * @param x The value to align
 * @return The aligned value
 */
constexpr uintptr_t ALIGN( uintptr_t x ) {
	return x & ~( PAGE_SIZE - 1 );
}

/**
 * @brief Check if bits are set in a value
 *
 * @param x The value to check
 * @param y the mask to check
 * @return A boolean that represent if the value matches the mask or not
 */
constexpr bool HAS_FLAGS( uint32_t x, uint32_t y ) {
	return ( x & y ) == y;
}

/**
 * @brief Check if an address is a user space virtual address
 *
 * @param VirtualAddress The address to check
 * @return A boolean that represent if the value is a user space virtual address or not
 */
constexpr bool IS_USER_VA( uintptr_t VirtualAddress ) {
	return VirtualAddress < 0x00007FFF'FFFF0000; // 0x00008000'00000000 - 64Kib No Access memory
}

#endif // !WINHVUTILS_HPP