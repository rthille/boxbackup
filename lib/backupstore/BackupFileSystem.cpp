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
#include "S3Client.h"

#include "MemLeakFindOn.h"

std::string S3BackupFileSystem::GetDirectoryURI(int64_t ObjectID)
{
	std::ostringstream out;
	out << mBasePath << "dirs/" << BOX_FORMAT_OBJECTID(ObjectID) << ".dir";
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

