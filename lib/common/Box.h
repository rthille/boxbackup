// --------------------------------------------------------------------------
//
// File
//		Name:    Box.h
//		Purpose: Main header file for the Box project
//		Created: 2003/07/08
//
// --------------------------------------------------------------------------

#ifndef BOX__H
#define BOX__H

// Use the same changes as gcc3 for gcc4
#ifdef PLATFORM_GCC4
	#define PLATFORM_GCC3
#endif

#include "BoxPlatform.h"

#include <memory>

// uncomment this line to enable full memory leak finding on all
// malloc-ed blocks (at least, ones used by the STL)
//#define MEMLEAKFINDER_FULL_MALLOC_MONITORING

#if !defined BOX_RELEASE_BUILD && !defined PLATFORM_DISABLE_MEM_LEAK_TESTING
	#define BOX_MEMORY_LEAK_TESTING
#endif

#ifdef BOX_MEMORY_LEAK_TESTING
	// Memory leak testing
	#include "MemLeakFinder.h"
	#define DEBUG_NEW new(__FILE__,__LINE__)
	#define MEMLEAKFINDER_NOT_A_LEAK(x)	memleakfinder_notaleak(x);
	#define MEMLEAKFINDER_NO_LEAKS		MemLeakSuppressionGuard _guard;
	#define MEMLEAKFINDER_INIT		memleakfinder_init();
	#define MEMLEAKFINDER_START {memleakfinder_global_enable = true;}
	#define MEMLEAKFINDER_STOP  {memleakfinder_global_enable = false;}
#else
	#define DEBUG_NEW new
	#define MEMLEAKFINDER_NOT_A_LEAK(x)
	#define MEMLEAKFINDER_NO_LEAKS
	#define MEMLEAKFINDER_INIT
	#define MEMLEAKFINDER_START
	#define MEMLEAKFINDER_STOP
#endif

// extra macros for converting to network byte order
#ifdef HAVE_NETINET_IN_H
	#include <netinet/in.h>
#endif

// Always define a swap64 function, as it's useful.
inline uint64_t box_swap64(uint64_t x)
{
	return ((x & 0xff) << 56 |
		(x & 0xff00LL) << 40 |
		(x & 0xff0000LL) << 24 |
		(x & 0xff000000LL) << 8 |
		(x & 0xff00000000LL) >> 8 |
		(x & 0xff0000000000LL) >> 24 |
		(x & 0xff000000000000LL) >> 40 |
		(x & 0xff00000000000000LL) >> 56);
}

#ifdef WORDS_BIGENDIAN
	#define box_hton64(x) (x)
	#define box_ntoh64(x) (x)
#elif defined(HAVE_BSWAP64)
	#ifdef HAVE_SYS_ENDIAN_H
		#include <sys/endian.h>
	#endif
	#ifdef HAVE_ASM_BYTEORDER_H
		#include <asm/byteorder.h>
	#endif

	#define box_hton64(x) BSWAP64(x)
	#define box_ntoh64(x) BSWAP64(x)
#else
	#define box_hton64(x) box_swap64(x)
	#define box_ntoh64(x) box_swap64(x)
#endif

// overloaded auto-conversion functions
inline uint64_t hton(uint64_t in) { return box_hton64(in); }
inline uint32_t hton(uint32_t in) { return htonl(in); }
inline uint16_t hton(uint16_t in) { return htons(in); }
inline uint8_t  hton(uint8_t in)  { return in; }
inline int64_t  hton(int64_t in)  { return box_hton64(in); }
inline int32_t  hton(int32_t in)  { return htonl(in); }
inline int16_t  hton(int16_t in)  { return htons(in); }
inline int8_t   hton(int8_t in)   { return in; }
inline uint64_t ntoh(uint64_t in) { return box_ntoh64(in); }
inline uint32_t ntoh(uint32_t in) { return ntohl(in); }
inline uint16_t ntoh(uint16_t in) { return ntohs(in); }
inline uint8_t  ntoh(uint8_t in)  { return in; }
inline int64_t  ntoh(int64_t in)  { return box_ntoh64(in); }
inline int32_t  ntoh(int32_t in)  { return ntohl(in); }
inline int16_t  ntoh(int16_t in)  { return ntohs(in); }
inline int8_t   ntoh(int8_t in)   { return in; }

#endif // BOX__H

