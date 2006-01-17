// distribution boxbackup-0.09
// 
//  
// Copyright (c) 2003, 2004
//      Ben Summers.  All rights reserved.
//  
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. All use of this software and associated advertising materials must 
//    display the following acknowledgement:
//        This product includes software developed by Ben Summers.
// 4. The names of the Authors may not be used to endorse or promote
//    products derived from this software without specific prior written
//    permission.
// 
// [Where legally impermissible the Authors do not disclaim liability for 
// direct physical injury or death caused solely by defects in the software 
// unless it is modified by a third party.]
// 
// THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//  
//  
//  
// --------------------------------------------------------------------------
//
// File
//		Name:    CollectInBufferStream.cpp
//		Purpose: Collect data in a buffer, and then read it out.
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------

#include "Box.h"

#include <string.h>

#include "CollectInBufferStream.h"
#include "CommonException.h"

#include "MemLeakFindOn.h"

#define INITIAL_BUFFER_SIZE	1024
#define MAX_BUFFER_ADDITION	(1024*64)

// --------------------------------------------------------------------------
//
// Function
//		Name:    CollectInBufferStream::CollectInBufferStream()
//		Purpose: Constructor
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------
CollectInBufferStream::CollectInBufferStream()
	: mBuffer(INITIAL_BUFFER_SIZE),
	  mBufferSize(INITIAL_BUFFER_SIZE),
	  mBytesInBuffer(0),
	  mReadPosition(0),
	  mInWritePhase(true)
{
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    CollectInBufferStream::~CollectInBufferStream()
//		Purpose: Destructor
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------
CollectInBufferStream::~CollectInBufferStream()
{
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    CollectInBufferStream::Read(void *, int, int)
//		Purpose: As interface. But only works in read phase
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------
int CollectInBufferStream::Read(void *pBuffer, int NBytes, int Timeout)
{
	if(mInWritePhase != false) { THROW_EXCEPTION(CommonException, CollectInBufferStreamNotInCorrectPhase) }
	
	// Adjust to number of bytes left
	if(NBytes > (mBytesInBuffer - mReadPosition))
	{
		NBytes = (mBytesInBuffer - mReadPosition);
	}
	ASSERT(NBytes >= 0);
	if(NBytes <= 0) return 0;	// careful now
	
	// Copy in the requested number of bytes and adjust the read pointer
	::memcpy(pBuffer, ((char*)mBuffer) + mReadPosition, NBytes);
	mReadPosition += NBytes;
	
	return NBytes;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    CollectInBufferStream::BytesLeftToRead()
//		Purpose: As interface. But only works in read phase
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------
IOStream::pos_type CollectInBufferStream::BytesLeftToRead()
{
	if(mInWritePhase != false) { THROW_EXCEPTION(CommonException, CollectInBufferStreamNotInCorrectPhase) }
	
	return (mBytesInBuffer - mReadPosition);
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    CollectInBufferStream::Write(void *, int)
//		Purpose: As interface. But only works in write phase
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------
void CollectInBufferStream::Write(const void *pBuffer, int NBytes)
{
	if(mInWritePhase != true) { THROW_EXCEPTION(CommonException, CollectInBufferStreamNotInCorrectPhase) }
	
	// Enough space in the buffer
	if((mBytesInBuffer + NBytes) > mBufferSize)
	{
		// Need to reallocate... what's the block size we'll use?
		int allocateBlockSize = mBufferSize;
		if(allocateBlockSize > MAX_BUFFER_ADDITION)
		{
			allocateBlockSize = MAX_BUFFER_ADDITION;
		}
		
		// Write it the easy way. Although it's not the most efficient...
		int newSize = mBufferSize;
		while(newSize < (mBytesInBuffer + NBytes))
		{
			newSize += allocateBlockSize;
		}
		
		// Reallocate buffer
		mBuffer.Resize(newSize);
		
		// Store new size
		mBufferSize = newSize;
	}
	
	// Copy in data and adjust counter
	::memcpy(((char*)mBuffer) + mBytesInBuffer, pBuffer, NBytes);
	mBytesInBuffer += NBytes;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    CollectInBufferStream::GetPosition()
//		Purpose: In write phase, returns the number of bytes written, in read
//				 phase, the number of bytes to go
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------
IOStream::pos_type CollectInBufferStream::GetPosition() const
{
	return mInWritePhase?mBytesInBuffer:mReadPosition;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    CollectInBufferStream::Seek(pos_type, int)
//		Purpose: As interface. But read phase only. 
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------
void CollectInBufferStream::Seek(pos_type Offset, int SeekType)
{
	if(mInWritePhase != false) { THROW_EXCEPTION(CommonException, CollectInBufferStreamNotInCorrectPhase) }
	
	int newPos = 0;
	switch(SeekType)
	{
	case IOStream::SeekType_Absolute:
		newPos = Offset;
		break;
	case IOStream::SeekType_Relative:
		newPos = mReadPosition + Offset;
		break;
	case IOStream::SeekType_End:
		newPos = mBytesInBuffer + Offset;
		break;
	default:
		THROW_EXCEPTION(CommonException, IOStreamBadSeekType)
		break;
	}
	
	// Make sure it doesn't go over
	if(newPos > mBytesInBuffer)
	{
		newPos = mBytesInBuffer;
	}
	// or under
	if(newPos < 0)
	{
		newPos = 0;
	}
	
	// Set the new read position
	mReadPosition = newPos;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    CollectInBufferStream::StreamDataLeft()
//		Purpose: As interface
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------
bool CollectInBufferStream::StreamDataLeft()
{
	return mInWritePhase?(false):(mReadPosition < mBytesInBuffer);
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    CollectInBufferStream::StreamClosed()
//		Purpose: As interface
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------
bool CollectInBufferStream::StreamClosed()
{
	return !mInWritePhase;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    CollectInBufferStream::SetForReading()
//		Purpose: Switch to read phase, after all data written
//		Created: 2003/08/26
//
// --------------------------------------------------------------------------
void CollectInBufferStream::SetForReading()
{
	if(mInWritePhase != true) { THROW_EXCEPTION(CommonException, CollectInBufferStreamNotInCorrectPhase) }
	
	// Move to read phase
	mInWritePhase = false;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    CollectInBufferStream::GetBuffer()
//		Purpose: Returns the buffer
//		Created: 2003/09/05
//
// --------------------------------------------------------------------------
void *CollectInBufferStream::GetBuffer() const
{
	return mBuffer.GetPtr();
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    CollectInBufferStream::GetSize()
//		Purpose: Returns the buffer size
//		Created: 2003/09/05
//
// --------------------------------------------------------------------------
int CollectInBufferStream::GetSize() const
{
	return mBytesInBuffer;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    CollectInBufferStream::Reset()
//		Purpose: Reset the stream, so it is empty and ready to be written to.
//		Created: 8/12/03
//
// --------------------------------------------------------------------------
void CollectInBufferStream::Reset()
{
	mInWritePhase = true;
	mBytesInBuffer = 0;
	mReadPosition = 0;
}

