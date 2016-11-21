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
	std::string mBasePath;
	S3Client& mrClient;
public:
	S3BackupFileSystem(const Configuration& config, const std::string& BasePath,
		S3Client& rClient)
	: mBasePath(BasePath),
	  mrClient(rClient)
	{ }
	std::string GetDirectoryURI(int64_t ObjectID);
	std::auto_ptr<HTTPResponse> GetDirectory(BackupStoreDirectory& rDir);
	int PutDirectory(BackupStoreDirectory& rDir);
};

#endif // BACKUPFILESYSTEM__H
