#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstddef>
#include <cstdint>

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

#endif // !UTILS_HPP