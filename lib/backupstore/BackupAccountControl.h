// --------------------------------------------------------------------------
//
// File
//		Name:    BackupAccountControl.h
//		Purpose: Client-side account management for Amazon S3 stores
//		Created: 2015/06/27
//
// --------------------------------------------------------------------------

#ifndef BACKUPACCOUNTCONTROL__H
#define BACKUPACCOUNTCONTROL__H

#include <string>

#include "BackupFileSystem.h"
#include "BackupStoreAccountDatabase.h"
#include "HTTPResponse.h"
#include "S3Client.h"

class BackupStoreDirectory;
class BackupStoreInfo;
class Configuration;
class UnixUser;
class NamedLock;

class BackupAccountControl
{
protected:
	const Configuration& mConfig;
	bool mMachineReadableOutput;
	std::auto_ptr<BackupStoreInfo> mapStoreInfo;
	std::auto_ptr<BackupFileSystem> mapFileSystem;

	virtual bool LoadBackupStoreInfo(bool readWrite) = 0;
	virtual std::string GetAccountIdentifier() = 0;
	virtual int GetBlockSize()
	{
		return mapFileSystem->GetBlockSize();
	}
	virtual int SetLimit(int64_t softlimit, int64_t hardlimit);

public:
	BackupAccountControl(const Configuration& config,
		bool machineReadableOutput = false)
	: mConfig(config),
	  mMachineReadableOutput(machineReadableOutput)
	{ }
	virtual ~BackupAccountControl() { }
	void CheckSoftHardLimits(int64_t SoftLimit, int64_t HardLimit);
	int64_t SizeStringToBlocks(const char *string, int BlockSize);
	std::string BlockSizeToString(int64_t Blocks, int64_t MaxBlocks, int BlockSize);
	virtual int SetLimit(const char *SoftLimitStr, const char *HardLimitStr);
	virtual int SetAccountName(const std::string& rNewAccountName);
	virtual int SetAccountEnabled(bool enabled);
	virtual BackupFileSystem& GetFileSystem() { return *mapFileSystem; }
};

class BackupStoreAccountControl : public BackupAccountControl
{
private:
	int32_t mAccountID;
	void OpenAccount(bool readWrite);
	std::string mRootDir;
	int mDiscSetNum;
	std::auto_ptr<UnixUser> mapChangeUser; // used to reset uid when we return

protected:
	virtual bool LoadBackupStoreInfo(bool readWrite);
	std::string GetAccountIdentifier();

public:
	BackupStoreAccountControl(const Configuration& config, int32_t AccountID,
		bool machineReadableOutput = false)
	: BackupAccountControl(config, machineReadableOutput),
	  mAccountID(AccountID),
	  mDiscSetNum(0)
	{ }
	virtual int GetBlockSize()
	{
		return BlockSizeOfDiscSet(mDiscSetNum);
	}
	int BlockSizeOfDiscSet(int discSetNum);
	int DeleteAccount(bool AskForConfirmation);
	int CheckAccount(bool FixErrors, bool Quiet,
		bool ReturnNumErrorsFound = false);
	int CreateAccount(int32_t DiscNumber, int32_t SoftLimit,
		int32_t HardLimit);
	int HousekeepAccountNow();
	virtual BackupFileSystem& GetFileSystem()
	{
		// We don't know whether the caller wants a write-locked filesystem or
		// not, but they can lock it themselves if they want to.
		OpenAccount(false); // !ReadWrite
		return *mapFileSystem;
	}
};

class S3BackupAccountControl : public BackupAccountControl
{
private:
	std::string mBasePath;
	std::auto_ptr<S3Client> mapS3Client;

protected:
	virtual bool LoadBackupStoreInfo(bool readWrite);
	std::string GetAccountIdentifier();

public:
	S3BackupAccountControl(const Configuration& config,
		bool machineReadableOutput = false);
	std::string GetFullPath(const std::string ObjectPath) const
	{
		return mBasePath + ObjectPath;
	}
	std::string GetFullURL(const std::string ObjectPath) const;
	int CreateAccount(const std::string& name, int32_t SoftLimit, int32_t HardLimit);
	int GetBlockSize() { return 4096; }
	HTTPResponse GetObject(const std::string& name)
	{
		return mapS3Client->GetObject(GetFullPath(name));
	}
	HTTPResponse PutObject(const std::string& name, IOStream& rStreamToSend,
		const char* pContentType = NULL)
	{
		return mapS3Client->PutObject(GetFullPath(name), rStreamToSend,
			pContentType);
	}
};

// max size of soft limit as percent of hard limit
#define MAX_SOFT_LIMIT_SIZE		97

#endif // BACKUPACCOUNTCONTROL__H

