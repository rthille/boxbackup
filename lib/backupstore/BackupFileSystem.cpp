// --------------------------------------------------------------------------
//
// File
//		Name:    BackupFileSystem.cpp
//		Purpose: Generic interface for reading and writing files and
//			 directories, abstracting over RaidFile, S3, FTP etc.
//		Created: 2015/08/03
//
// --------------------------------------------------------------------------

#include "Box.h"

#include <sys/types.h>

#include <sstream>

#include "BackupStoreDirectory.h"
#include "BackupFileSystem.h"
#include "CollectInBufferStream.h"
#include "Configuration.h"
#include "S3Client.h"
#include "StoreStructure.h"

#include "MemLeakFindOn.h"

S3BackupFileSystem::S3BackupFileSystem(const Configuration& config,
	const std::string& BasePath, S3Client& rClient)
: mrConfig(config),
  mBasePath(BasePath),
  mrClient(rClient)
{ }

std::string S3BackupFileSystem::GetObjectURL(const std::string& ObjectURI) const
{
	const Configuration s3config = mrConfig.GetSubConfiguration("S3Store");
	return std::string("http://") + s3config.GetKeyValue("HostName") + ":" +
		s3config.GetKeyValue("Port") + ObjectURI;
}

// --------------------------------------------------------------------------
//
// Function
//		Name:    S3BackupFileSystem::GetObjectURI(int64_t ObjectID,
//			 int Type)
//		Purpose: Builds the object filename for a given object,
//			 including mBasePath. Very similar to
//			 StoreStructure::MakeObjectFilename(), but files and
//			 directories have different extensions, and the
//			 filename is the full object ID, not just the lower
//			 STORE_ID_SEGMENT_LENGTH bits.
//		Created: 2016/03/21
//
// --------------------------------------------------------------------------
std::string S3BackupFileSystem::GetObjectURI(int64_t ObjectID, int Type)
{
	const static char *hex = "0123456789abcdef";
	std::ostringstream out;
	ASSERT(mBasePath.size() > 0 && mBasePath[0] == '/' &&
		mBasePath[mBasePath.size() - 1] == '/');
	out << mBasePath;

	// Get the id value from the stored object ID into an unsigned int64_t, so that
	// we can do bitwise operations on it safely.
	uint64_t id = (uint64_t)ObjectID;

	// Shift off the bits which make up the leafname
	id >>= STORE_ID_SEGMENT_LENGTH;

	// build pathname
	while(id != 0)
	{
		// assumes that the segments are no bigger than 8 bits
		int v = id & STORE_ID_SEGMENT_MASK;
		out << hex[(v & 0xf0) >> 4];
		out << hex[v & 0xf];
		out << "/";

		// shift the bits we used off the pathname
		id >>= STORE_ID_SEGMENT_LENGTH;
	}

	// append the filename
	out << BOX_FORMAT_OBJECTID(ObjectID);
	if(Type == ObjectExists_File)
	{
		out << ".file";
	}
	else if(Type == ObjectExists_Dir)
	{
		out << ".dir";
	}
	else
	{
		THROW_EXCEPTION_MESSAGE(CommonException, Internal,
			"Unknown file type for object " << BOX_FORMAT_OBJECTID(ObjectID) <<
			": " << Type);
	}

	return out.str();
}

std::auto_ptr<HTTPResponse> S3BackupFileSystem::GetDirectory(BackupStoreDirectory& rDir)
{
	std::string uri = GetDirectoryURI(rDir.GetObjectID());
	HTTPResponse response = mrClient.GetObject(uri);
	mrClient.CheckResponse(response,
		std::string("Failed to download directory: ") + uri);
	return std::auto_ptr<HTTPResponse>(new HTTPResponse(response));
}

int S3BackupFileSystem::PutDirectory(BackupStoreDirectory& rDir)
{
	CollectInBufferStream out;
	rDir.WriteToStream(out);
	out.SetForReading();

	std::string uri = GetDirectoryURI(rDir.GetObjectID());
	HTTPResponse response = mrClient.PutObject(uri, out);
	mrClient.CheckResponse(response,
		std::string("Failed to upload directory: ") + uri);

	int blocks = (out.GetSize() + S3_NOTIONAL_BLOCK_SIZE - 1) / S3_NOTIONAL_BLOCK_SIZE;
	return blocks;
}

