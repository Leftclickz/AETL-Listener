#pragma once
#include <string>
#include <vector>
#include <thread>
#include <filesystem>
#include <iostream>
#include <Windows.h>
#include "sqllite/SQL_Helpers.h"

class Project
{
public:
	static SQL::ProjectSQLData SQL_DATA;
	static SQL::RenderData RENDER_DATA;
	static std::string PROJECT_NAME;
	static std::string CURRENT_RENDERING_DIRECTORY;
	static std::filesystem::path CURRENT_PROJECT_PATH;

	static std::string TIMESTAMP;
	static std::string TIMESTAMPED_FILENAME;

	static std::string FINAL_RENDER_FILEPATH;

	static sqlite3* OUR_DATABASE;

	static void Reset();
};

class Dir
{
public:
	static std::string HotFolder;//the folder we watch
	static std::string RenderPath;//the path to adobe render exe
	static std::string DatabasePath;//path to the database
	static std::string OutputFolder;//path to the output folder
	static std::string EncodeFolder;//path to the AETL-Encoder
	static double PercentThreshold;//threshold to use

	static int ADOBE_VERSION;
	static bool UsingSqlite;

	static std::vector<std::string> ResolutionsToEncode;
};

#define ENSURE_DRIVE_SAFETY while (DrivesAreAccessible() == false) SLEEP(10000);;
#define PRINT_FS_ERROR(_EC_) ;if (_EC_.value() > 0) {std::cout << _EC_.message() << std::endl; LogFile::WriteToLog("FILESYSTEM ERROR: " + _EC_.message()); }
#define EXIT_ON_FS_ERROR(_EC_) ;if (_EC_.value() > 0) {std::cout << _EC_.message() << std::endl; LogFile::WriteToLog("FILESYSTEM ERROR: " + _EC_.message()); return _EC_.value(); }
#define EXIT_ON_ERROR(_VAL_) ;if (_VAL_ != 0) return;
#define SLEEP(_VAL) std::this_thread::sleep_for(std::chrono::milliseconds(_VAL))

//listener file def
#define LISTENER_FILE_NAME string("_DO_NOT_READ_")

//Archive folder def
#define ARCHIVE_DIRECTORY "Archive"

//Active rendering folder ref
#define ACTIVE_RENDER_DIECTORY "Rendering"

//Failed folder def
#define FAIL_DIRECTORY "Failed"

#define FAIL_FILE ".log"

//Replace an occurence with something else
void FindAndReplaceAll(std::string& data, std::string toSearch, std::string replaceStr);

//Get a datetime stamp
const std::string CurrentDateTime();

//get our directory
const std::string GetAbsoluteDirectory(std::string Directory);

//Import a bunch of files
void RenameMultipleFilesToNewDirectory(std::vector<std::string> fileNames, std::string OldDirectory, std::string NewDirectory);

//Check if a directory exists. Can create it if it doesn't exist.
bool DirectoryExists(std::string FolderPath, bool CreateDirectoryIfDoesNotExist = true);

int CreateDirectory(std::string Path);

void AppendHostName(std::string& data);

bool EnsureSafeExecution(void(*FUNC) (void*, void*, int*), void* data_in = nullptr, void* data_out = nullptr);

bool DrivesAreAccessible();

struct DiskInfo
{
	ULARGE_INTEGER TotalNumberOfBytes;
	ULARGE_INTEGER FreeBytesAvailable;
	ULARGE_INTEGER TotalNumberOfFreeBytes;

	double FreeSpaceInGigaBytes = 0;
	double PercentUsed = 0;
};

namespace UNSAFE
{
	void RunOnceProgramSetup(void* data_in, void* data_out, int* ret);

	void FetchProjectBuildLogUnsafe(void* data_in, void* data_out, int* ret);

	void CollectActiveRenderingDataUnsafe(void* data_in, void* data_out, int* ret);

	void AdjustActiveRenderingDataUnsafe(void* data_in, void* data_out, int* ret);

	void AddActiveRenderingDataUnsafe(void* data_in, void* data_out, int* ret);

	void AddRenderLogUnsafe(void* data_in, void* data_out, int* ret);

	void ObjectExistsUnsafe(void* data_in, void* data_out, int* ret);

	void CreateDirectoryUnsafe(void* data_in, void* data_out, int* ret);
	
	void RenameFileUnsafe(void* data_old_path, void* data_new_path, int* ret);

	void IsDirectoryUnsafe(void* data_in, void* data_out, int* ret);

	void RemoveDirectoryUnsafe(void* data_in, void* data_out, int* ret);

	void AviCleanupUnsafe(void* data_in, void* data_out, int* ret);

	void EncodeCleanup(void* data_in, void* data_out, int* ret);

	void DeleteAllLockfilesForProject(void* data_in, void* data_out, int* ret);

	void GetDirectoryIterator(void* data_in, void* data_out, int* ret);

	void CreateOutputLogUnsafe(void* data_filename, void* data_to_write, int* ret);

	void AttemptVideoRender(void* data_in, void* data_out, int* ret);

	void AttemptVideoEncode(void* data_in, void* data_out, int* ret);

	void FreeSpaceAvailable(void* data_in, void* data_out, int* ret);

	void CheckIfDrivesAreAccessible(void* data_in, void* data_out, int* ret);

	void IsHotFolderLocked(void* data_in, void* data_out, int* ret);
}

//Remove extra data
void StripExtraData(std::string& Directory);