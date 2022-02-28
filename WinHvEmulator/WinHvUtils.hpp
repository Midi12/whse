#ifndef WINHVUTILS_HPP
#define WINHVUTILS_HPP

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

constexpr size_t PAGE_SIZE = 4096;

constexpr size_t ALIGN_PAGE( size_t x ) {
	return ( ( PAGE_SIZE - 1 ) & x ) ? ( ( x + PAGE_SIZE ) & ~( PAGE_SIZE - 1 ) ) : x;
}

constexpr bool HAS_FLAGS( uint32_t x, uint32_t y ) {
	return ( x & y ) == y;
}

constexpr bool IS_USER_VA( uintptr_t VirtualAddress ) {
	return VirtualAddress < 0x00007FFF'FFFF0000; // 0x00008000'00000000 - 64Kib No Access memory
}

#endif // !WINHVUTILS_HPP