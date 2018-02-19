// --------------------------------------------------------------------------
//
// File
//		Name:    RaidFileUtil.cpp
//		Purpose: Utilities for raid files
//		Created: 2003/07/11
//
// --------------------------------------------------------------------------

#include "Box.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <string>

#include "RaidFileUtil.h"
#include "FileModificationTime.h"
#include "RaidFileRead.h" // for type definition

#include "MemLeakFindOn.h"

int64_t adjust_timestamp(int64_t timestamp, size_t file_size)
{
#ifndef BOX_RELEASE_BUILD
	// Remove the microseconds part of the timestamp,
	// to simulate filesystem with 1-second timestamp
	// resolution, e.g. MacOS X HFS, old Linuxes.
	// Otherwise it's easy to write tests that rely
	// on more accurate timestamps, and pass on
	// platforms that have them, and fail on others.
	timestamp -= (timestamp % MICRO_SEC_IN_SEC);
#endif

	// The resolution of timestamps may be very
	// low, e.g. 1 second. So add the size to it
	// to give a bit more chance of it changing.
	// TODO: Make this better.
	timestamp += file_size;

	return timestamp;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    RaidFileUtil::RaidFileExists(RaidFileDiscSet &,
//			 const std::string &, int *, int *, int64_t *)
//		Purpose: Check to see the state of a RaidFile on disc
//			 (doesn't look at contents, just at existence of
//			 files)
//		Created: 2003/07/11
//
// --------------------------------------------------------------------------
RaidFileUtil::ExistType RaidFileUtil::RaidFileExists(RaidFileDiscSet &rDiscSet,
	const std::string &rFilename, int *pStartDisc, int *pExistingFiles,
	int64_t *pRevisionID)
{
	if(pExistingFiles)
	{
		*pExistingFiles = 0;
	}
	
	EMU_STRUCT_STAT st;

	// check various files
	int startDisc = 0;
	{
		std::string writeFile(RaidFileUtil::MakeWriteFileName(rDiscSet, rFilename, &startDisc));
		if(pStartDisc)
		{
			*pStartDisc = startDisc;
		}
		if(EMU_STAT(writeFile.c_str(), &st) == 0)
		{
			// write file exists, use that
			
			// Get unique ID
			if(pRevisionID != 0)
			{
				*pRevisionID = FileModificationTime(st);
				*pRevisionID = adjust_timestamp(*pRevisionID, st.st_size);
			}
			
			// return non-raid file
			return NonRaid;
		}
	}
	
	// Now see how many of the raid components exist
	int64_t revisionID = 0;
	int setSize = rDiscSet.size();
	int rfCount = 0;

	// TODO: replace this with better linux revision ID detection
	int64_t revisionIDplus = 0;

	for(int f = 0; f < setSize; ++f)
	{
		std::string componentFile(RaidFileUtil::MakeRaidComponentName(rDiscSet, rFilename, (f + startDisc) % setSize));
		if(EMU_STAT(componentFile.c_str(), &st) == 0)
		{
			// Component file exists, add to count
			rfCount++;
			// Set flags for existance?
			if(pExistingFiles)
			{
				(*pExistingFiles) |= (1 << f);
			}
			// Revision ID
			if(pRevisionID != 0)
			{
				int64_t rid = FileModificationTime(st);
				if(rid > revisionID) revisionID = rid;
				revisionIDplus += st.st_size;
			}
		}
	}
	if(pRevisionID != 0)
	{
		revisionID = adjust_timestamp(revisionID, revisionIDplus);
		(*pRevisionID) = revisionID;
	}
	
	// Return a status based on how many parts are available
	if(rfCount == setSize)
	{
		return AsRaid;
	}
	else if((setSize > 1) && rfCount == (setSize - 1))
	{
		return AsRaidWithMissingReadable;
	}
	else if(rfCount > 0)
	{
		return AsRaidWithMissingNotRecoverable;
	}
	
	return NoFile;	// Obviously doesn't exist
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    RaidFileUtil::DiscUsageInBlocks(int64_t, const RaidFileDiscSet &)
//		Purpose: Returns the size of the file in blocks, given the file size and disc set
//		Created: 2003/09/03
//
// --------------------------------------------------------------------------
int64_t RaidFileUtil::DiscUsageInBlocks(int64_t FileSize, const RaidFileDiscSet &rDiscSet)
{
	// Get block size
	int blockSize = rDiscSet.GetBlockSize();

	// OK... so as the size of the file is always sizes of stripe1 + stripe2, we can
	// do a very simple calculation for the main data.
	int64_t blocks = (FileSize + (((int64_t)blockSize) - 1)) / ((int64_t)blockSize);
	
	// It's just that simple calculation for non-RAID disc sets
	if(rDiscSet.IsNonRaidSet())
	{
		return blocks;
	}

	// It's the parity which is mildly complex.
	// First of all, add in size for all but the last two blocks.
	int64_t parityblocks = (FileSize / ((int64_t)blockSize)) / 2;
	blocks += parityblocks;
	
	// Work out how many bytes are left
	int bytesOver = (int)(FileSize - (parityblocks * ((int64_t)(blockSize*2))));
	
	// Then... (let compiler optimise this out)
	if(bytesOver == 0)
	{
		// Extra block for the size info
		blocks++;
	}
	else if(bytesOver == sizeof(RaidFileRead::FileSizeType))
	{
		// For last block of parity, plus the size info
		blocks += 2;
	}
	else if(bytesOver < blockSize)
	{
		// Just want the parity block
		blocks += 1;
	}
	else if(bytesOver == blockSize || bytesOver >= ((blockSize*2)-((int)sizeof(RaidFileRead::FileSizeType))))
	{
		// Last block, plus size info
		blocks += 2;
	}
	else
	{
		// Just want parity block
		blocks += 1;
	}
	
	return blocks;
}


