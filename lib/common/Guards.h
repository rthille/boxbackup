// --------------------------------------------------------------------------
//
// File
//		Name:    Guards.h
//		Purpose: Classes which ensure things are closed/deleted properly when
//				 going out of scope. Easy exception proof code, etc
//		Created: 2003/07/12
//
// --------------------------------------------------------------------------

#ifndef GUARDS__H
#define GUARDS__H

#include "Box.h"

#ifdef HAVE_UNISTD_H
	#include <unistd.h>
#endif

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <new>

#include "CommonException.h"

#include "MemLeakFindOn.h"

#ifdef WIN32
	#define INVALID_FILE NULL
	typedef HANDLE tOSFileHandle;
#else
	#define INVALID_FILE -1
	typedef int tOSFileHandle;
#endif

template <int flags = O_RDONLY, int mode = (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)>
class FileHandleGuard
{
public:
	FileHandleGuard(const char *filename)
#ifdef WIN32
		: mOSFileHandle(::openfile(filename, flags, mode))
#else
		: mOSFileHandle(::open(filename, flags, mode))
#endif
	{
		if(mOSFileHandle < 0)
		{
			THROW_EXCEPTION(CommonException, OSFileOpenError)
		}
	}
	
	~FileHandleGuard()
	{
		if(mOSFileHandle == INVALID_FILE)
		{
			Close();
		}
	}
	
	void Close()
	{
		if(mOSFileHandle == INVALID_FILE)
		{
			THROW_EXCEPTION(CommonException, FileAlreadyClosed)
		}
#ifdef WIN32
		if(::CloseHandle(mOSFileHandle) == 0)
#else
		if(::close(mOSFileHandle) != 0)
#endif
		{
			THROW_EXCEPTION(CommonException, OSFileCloseError)
		}
		mOSFileHandle = INVALID_FILE;
	}
	
	operator int() const
	{
		return (int)mOSFileHandle;
	}

private:
	tOSFileHandle mOSFileHandle;
};

template<typename type>
class MemoryBlockGuard
{
public:
	MemoryBlockGuard(int BlockSize)
		: mpBlock(::malloc(BlockSize))
	{
		if(mpBlock == 0)
		{
			throw std::bad_alloc();
		}
	}
	
	~MemoryBlockGuard()
	{
		free(mpBlock);
	}
	
	operator type() const
	{
		return (type)mpBlock;
	}
	
	type GetPtr() const
	{
		return (type)mpBlock;
	}
	
	void Resize(int NewSize)
	{
		void *ptrn = ::realloc(mpBlock, NewSize);
		if(ptrn == 0)
		{
			throw std::bad_alloc();
		}
		mpBlock = ptrn;
	}
	
private:
	void *mpBlock;
};

#include "MemLeakFindOff.h"

#endif // GUARDS__H

