#ifndef WINHVEXPORTS_HPP
#define WINHVEXPORTS_HPP

#if defined( WHSE_EXPORTS )
#define WHSEAPI __declspec(dllexport)
#elif defined( WINHVEMULATOR_IMPORTS )
#define WHSEAPI __declspec(dllimport)
#else
#define WHSEAPI
#endif

#endif // !WINHVEXPORTS_HPP