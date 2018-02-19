// --------------------------------------------------------------------------
//
// File
//		Name:    MemLeakFindOn.h
//		Purpose: Switch memory leak finding on
//		Created: 13/1/04
//
// --------------------------------------------------------------------------

// no header guard

#ifdef BOX_MEMORY_LEAK_TESTING

#	include "MemLeakFinder.h"

#	define new DEBUG_NEW

#	ifndef MEMLEAKFINDER_MALLOC_MONITORING_DEFINED
#		define malloc(X)	memleakfinder_malloc(X, __FILE__, __LINE__)
#		define calloc(X, Y)	memleakfinder_calloc(X, Y, __FILE__, __LINE__)
#		define realloc		memleakfinder_realloc
#		define free		memleakfinder_free
#		define MEMLEAKFINDER_MALLOC_MONITORING_DEFINED
#	endif

#	define MEMLEAKFINDER_ENABLED

#endif
