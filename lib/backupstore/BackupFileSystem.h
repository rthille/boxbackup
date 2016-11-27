// --------------------------------------------------------------------------
//
// File
//		Name:    BackupFileSystem.h
//		Purpose: Generic interface for reading and writing files and
//			 directories, abstracting over RaidFile, S3, FTP etc.
//		Created: 2015/08/03
//
// --------------------------------------------------------------------------

#ifndef BACKUPFILESYSTEM__H
#define BACKUPFILESYSTEM__H

#include <string>

#include "HTTPResponse.h"
#include "S3Client.h"
#include "Utils.h" // for ObjectExists_File and ObjectExists_Dir

class BackupStoreDirectory;
class BackupStoreInfo;
class Configuration;
class IOStream;

// max size of soft limit as percent of hard limit
#define MAX_SOFT_LIMIT_SIZE		97
#define S3_INFO_FILE_NAME		"boxbackup.info"
#define S3_NOTIONAL_BLOCK_SIZE		1048576

// --------------------------------------------------------------------------
//
// Class
//		Name:    BackupFileSystem
//		Purpose: Generic interface for reading and writing files and
//			 directories, abstracting over RaidFile, S3, FTP etc.
//		Created: 2015/08/03
//
// --------------------------------------------------------------------------

class S3BackupFileSystem
{
private:
	const Configuration& mrConfig;
	std::string mBasePath;
	S3Client& mrClient;

public:
	S3BackupFileSystem(const Configuration& config, const std::string& BasePath,
		S3Client& rClient);

	// These are public to help with writing tests ONLY:
	// The returned URI should start with mBasePath.
	std::string GetMetadataURI(const std::string& MetadataFilename) const
	{
		return mBasePath + MetadataFilename;
	}
	// The returned URI should start with mBasePath.
	std::string GetDirectoryURI(int64_t ObjectID)
	{
		return GetObjectURI(ObjectID, ObjectExists_Dir);
	}
	// The returned URI should start with mBasePath.
	std::string GetFileURI(int64_t ObjectID)
	{
		return GetObjectURI(ObjectID, ObjectExists_File);
	}

	std::auto_ptr<HTTPResponse> GetDirectory(BackupStoreDirectory& rDir);
	int PutDirectory(BackupStoreDirectory& rDir);

private:
	// GetObjectURL() returns the complete URL for an object at the given
	// path, by adding the hostname, port and the object's URI (which can
	// be retrieved from GetMetadataURI or GetObjectURI). This is only used
	// for error messages, since S3Client doesn't take URLs.
	std::string GetObjectURL(const std::string& ObjectURI) const;

	// GetObjectURI() is a private interface which converts an object ID
	// and type into a URI, which starts with mBasePath:
	std::string GetObjectURI(int64_t ObjectID, int Type);
};

#endif // BACKUPFILESYSTEM__H
