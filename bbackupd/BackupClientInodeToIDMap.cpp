// --------------------------------------------------------------------------
//
// File
//		Name:    BackupClientInodeToIDMap.cpp
//		Purpose: Map of inode numbers to file IDs on the store
//		Created: 11/11/03
//
// --------------------------------------------------------------------------

#include "Box.h"

#ifdef HAVE_DB
	// Include db headers and other OS files if they're needed for the disc implementation
	#include <sys/types.h>
	#include <fcntl.h>
	#include <limits.h>
	#include DB_HEADER
	#include <sys/stat.h>
#endif

#define BACKIPCLIENTINODETOIDMAP_IMPLEMENTATION
#include "BackupClientInodeToIDMap.h"

#include "BackupStoreException.h"


#include "MemLeakFindOn.h"

// What type of Berkeley DB shall we use?
#define TABLE_DATABASE_TYPE DB_HASH

typedef struct
{
	int64_t mObjectID;
	int64_t mInDirectory;
} IDBRecord;

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupClientInodeToIDMap::BackupClientInodeToIDMap()
//		Purpose: Constructor
//		Created: 11/11/03
//
// --------------------------------------------------------------------------
BackupClientInodeToIDMap::BackupClientInodeToIDMap()
#ifndef BACKIPCLIENTINODETOIDMAP_IN_MEMORY_IMPLEMENTATION
	: mReadOnly(true),
	  mEmpty(false),
	  dbp(0)
#endif
{
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupClientInodeToIDMap::~BackupClientInodeToIDMap()
//		Purpose: Destructor
//		Created: 11/11/03
//
// --------------------------------------------------------------------------
BackupClientInodeToIDMap::~BackupClientInodeToIDMap()
{
#ifndef BACKIPCLIENTINODETOIDMAP_IN_MEMORY_IMPLEMENTATION
	if(dbp != 0)
	{
		dbp->close(dbp);
	}
#endif
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupClientInodeToIDMap::Open(const char *, bool, bool)
//		Purpose: Open the database map, creating a file on disc to store everything
//		Created: 20/11/03
//
// --------------------------------------------------------------------------
void BackupClientInodeToIDMap::Open(const char *Filename, bool ReadOnly, bool CreateNew)
{
#ifndef BACKIPCLIENTINODETOIDMAP_IN_MEMORY_IMPLEMENTATION
	// Correct arguments?
	ASSERT(!(CreateNew && ReadOnly));
	
	// Correct usage?
	ASSERT(dbp == 0);
	ASSERT(!mEmpty);
	
	// Open the database file
	dbp = dbopen(Filename, (CreateNew?O_CREAT:0) | (ReadOnly?O_RDONLY:O_RDWR), S_IRUSR | S_IWUSR | S_IRGRP, TABLE_DATABASE_TYPE, NULL);
	if(dbp == NULL)
	{
		THROW_EXCEPTION(BackupStoreException, BerkelyDBFailure);
	}
	
	// Read only flag
	mReadOnly = ReadOnly;
#endif
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupClientInodeToIDMap::OpenEmpty()
//		Purpose: 'Open' this map. Not associated with a disc file. Useful for when a map
//				 is required, but is against an empty file on disc which shouldn't be created.
//				 Implies read only.
//		Created: 20/11/03
//
// --------------------------------------------------------------------------
void BackupClientInodeToIDMap::OpenEmpty()
{
#ifndef BACKIPCLIENTINODETOIDMAP_IN_MEMORY_IMPLEMENTATION
	ASSERT(dbp == 0);
	mEmpty = true;
	mReadOnly = true;
#endif
}



// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupClientInodeToIDMap::Close()
//		Purpose: Close the database file
//		Created: 20/11/03
//
// --------------------------------------------------------------------------
void BackupClientInodeToIDMap::Close()
{
#ifndef BACKIPCLIENTINODETOIDMAP_IN_MEMORY_IMPLEMENTATION
	if(dbp != 0)
	{
		if(dbp->close(dbp) != 0)
		{
			THROW_EXCEPTION(BackupStoreException, BerkelyDBFailure);
		}
		dbp = 0;
	}
#endif
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupClientInodeToIDMap::AddToMap(InodeRefType, int64_t, int64_t)
//		Purpose: Adds an entry to the map. Overwrites any existing entry.
//		Created: 11/11/03
//
// --------------------------------------------------------------------------
void BackupClientInodeToIDMap::AddToMap(InodeRefType InodeRef, int64_t ObjectID, int64_t InDirectory)
{
#ifdef BACKIPCLIENTINODETOIDMAP_IN_MEMORY_IMPLEMENTATION
	mMap[InodeRef] = std::pair<int64_t, int64_t>(ObjectID, InDirectory);
#else
	if(mReadOnly)
	{
		THROW_EXCEPTION(BackupStoreException, InodeMapIsReadOnly);
	}

	if(dbp == 0)
	{
		THROW_EXCEPTION(BackupStoreException, InodeMapNotOpen);
	}

	// Setup structures
	IDBRecord rec;
	rec.mObjectID = ObjectID;
	rec.mInDirectory = InDirectory;
	
	DBT key;
	key.data = &InodeRef;
	key.size = sizeof(InodeRef);
	
	DBT data;
	data.data = &rec;
	data.size = sizeof(rec);
	
	// Add to map (or replace existing entry)
	if(dbp->put(dbp, &key, &data, 0) != 0)
	{
		THROW_EXCEPTION(BackupStoreException, BerkelyDBFailure);
	}
#endif
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    BackupClientInodeToIDMap::Lookup(InodeRefType, int64_t &, int64_t &) const
//		Purpose: Looks up an inode in the map, returning true if it exists, and the object
//				 ids of it and the directory it's in the reference arguments.
//		Created: 11/11/03
//
// --------------------------------------------------------------------------
bool BackupClientInodeToIDMap::Lookup(InodeRefType InodeRef, int64_t &rObjectIDOut, int64_t &rInDirectoryOut) const
{
#ifdef BACKIPCLIENTINODETOIDMAP_IN_MEMORY_IMPLEMENTATION
	std::map<InodeRefType, std::pair<int64_t, int64_t> >::const_iterator i(mMap.find(InodeRef));
	
	// Found?
	if(i == mMap.end())
	{
		return false;
	}

	// Yes. Return the details
	rObjectIDOut = i->second.first;
	rInDirectoryOut = i->second.second;
	return true;
#else
	if(mEmpty)
	{
		// Map is empty
		return false;
	}

	if(dbp == 0)
	{
		THROW_EXCEPTION(BackupStoreException, InodeMapNotOpen);
	}

	DBT key;
	key.data = &InodeRef;
	key.size = sizeof(InodeRef);
	
	DBT data;
	data.data = 0;
	data.size = 0;

	switch(dbp->get(dbp, &key, &data, 0))
	{
	case 1:	// key not in file
		return false;
	
	case -1:	// error
	default:	// not specified in docs
		THROW_EXCEPTION(BackupStoreException, BerkelyDBFailure);
		return false;
	
	case 0:		// success, found it
		break;		
	}

	// Check for sensible return
	if(key.data == 0 || data.size != sizeof(IDBRecord))
	{
		// Assert in debug version
		ASSERT(key.data == 0 || data.size != sizeof(IDBRecord));
		
		// Invalid entries mean it wasn't found
		return false;
	}
	
	// Data alignment isn't guarentted to be on a suitable bounday
	IDBRecord rec;
	::memcpy(&rec, data.data, sizeof(rec));
	
	// Return data
	rObjectIDOut = rec.mObjectID;
	rInDirectoryOut = rec.mInDirectory;

	// Don't have to worry about freeing the returned data

	// Found
	return true;
#endif
}


