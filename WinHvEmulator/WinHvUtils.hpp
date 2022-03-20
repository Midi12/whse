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


#define MAKE_SEGMENT(reg, sel, base, limit, dpl, l, gr)			\
	registers[ reg ].Segment.Selector = ( sel | dpl );			\
	registers[ reg ].Segment.Base = base;						\
	registers[ reg ].Segment.Limit = limit;						\
	registers[ reg ].Segment.Present = 1;						\
	registers[ reg ].Segment.DescriptorPrivilegeLevel = dpl;	\
	registers[ reg ].Segment.Long = l;							\
	registers[ reg ].Segment.Granularity = gr

#define MAKE_CR0(pe, mp, em, ts, et, ne, wp, am, nw, cd, pg)	\
	(															\
		(														\
			( pg << 31 ) |										\
			( cd << 30 ) |										\
			( nw << 29 ) |										\
			( am << 18 ) |										\
			( wp << 16 ) |										\
			( ne << 5 ) |										\
			( et << 4 ) |										\
			( ts << 3 ) |										\
			( em << 2 ) |										\
			( mp << 1 ) |										\
			( pe << 0 )											\
		) & UINT32_MAX											\
	)

#define MAKE_CR4(vme, pvi, tsd, de, pse, pae, mce, pge, pce, osfxsr, osxmmexcpt, umip, la57, vmxe, smxe, fsgsbase, pcide, osxsave, smep, smap, pke, cet, pks) \
	(															\
		(														\
			( pks			<< 24 ) |							\
			( cet			<< 23 ) |							\
			( pke			<< 22 ) |							\
			( smap			<< 21 ) |							\
			( smep			<< 20 ) |							\
			( osxsave		<< 18 ) |							\
			( pcide			<< 17 ) |							\
			( fsgsbase		<< 16 ) |							\
			( smxe			<< 14 ) |							\
			( vmxe			<< 13 ) |							\
			( la57			<< 12 ) |							\
			( umip			<< 11 ) |							\
			( osxmmexcpt	<< 10 ) |							\
			( osfxsr		<< 9 ) |							\
			( pce			<< 8 ) |							\
			( pge			<< 7 ) |							\
			( mce			<< 6 ) |							\
			( pae			<< 5 ) |							\
			( pse			<< 4 ) |							\
			( de			<< 3 ) |							\
			( tsd			<< 2 ) |							\
			( pvi			<< 1 ) |							\
			( vme			<< 0 )								\
		) & ( ~( 1 << 24 ) )									\
	)

#define MAKE_EFER(sce, dpe, sewbed, gewbed, l2d, lme, lma, nxe, svme, lmsle, ffxsr, tce) \
	(															\
		(														\
			( tce		<< 15) |								\
			( ffxsr		<< 14) |								\
			( lmsle		<< 13) |								\
			( svme		<< 12) |								\
			( nxe		<< 11) |								\
			( lma		<< 10) |								\
			( lme		<< 8) |									\
			( l2d		<< 4) |									\
			( gewbed	<< 3) |									\
			( sewbed	<< 2) |									\
			( dpe		<< 1) |									\
			( sce		<< 0 )									\
		) & ( ~( 1 << 16 ) )									\
	)

#define MAKE_RFLAGS(cf, pf, af, zf, sf, tf, if_, df, of, iopl, nt, rf, vm, ac, vif, vip, id) \
	(															\
		(														\
			( id	<< 21 ) |									\
			( vip	<< 20 ) |									\
			( vif	<< 19 ) |									\
			( ac	<< 18 ) |									\
			( vm	<< 17 ) |									\
			( rf	<< 16 ) |									\
			( nt	<< 14 ) |									\
			( iopl	<< 12 ) |									\
			( of	<< 11 ) |									\
			( df	<< 10 ) |									\
			( if_	<< 9 ) |									\
			( tf	<< 8 ) |									\
			( sf	<< 7 ) |									\
			( zf	<< 6 ) |									\
			( af	<< 4 ) |									\
			( pf	<< 2 ) |									\
			(  1	<< 1 ) |									\
			( cf	<< 0 )										\
		) & ( ~( 1 << 21 ) )									\
	)

#endif // !WINHVUTILS_HPP