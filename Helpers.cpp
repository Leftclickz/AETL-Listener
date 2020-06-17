#include "ListenerPCH.h"

#include <time.h>
#include <fstream>
#include <filesystem>
#include <winsock.h>
#include <iostream>


using namespace std;

SQL::ProjectSQLData Project::SQL_DATA;
SQL::RenderData Project::RENDER_DATA;
std::string Project::PROJECT_NAME;
sqlite3* Project::OUR_DATABASE;

std::filesystem::path Project::CURRENT_PROJECT_PATH = "";

std::string Project::TIMESTAMP = "";
std::string Project::TIMESTAMPED_FILENAME = "";
std::string Project::FINAL_RENDER_FILEPATH = "";

std::string Project::CURRENT_RENDERING_DIRECTORY = "";

void Project::Reset()
{
	CURRENT_PROJECT_PATH = "";
	PROJECT_NAME = "";
	CURRENT_RENDERING_DIRECTORY = "";
	TIMESTAMP = "";
	TIMESTAMPED_FILENAME = "";
	FINAL_RENDER_FILEPATH = "";
	SQL_DATA.Reset();
	RENDER_DATA.Reset();
}

std::string Dir::HotFolder = "";
std::string Dir::RenderPath = "";
std::string Dir::DatabasePath = "";
std::string Dir::OutputFolder = "";
std::string Dir::EncodeFolder = "";

double Dir::PercentThreshold = 35.0;
int Dir::ADOBE_VERSION = 2018;

bool Dir::UsingSqlite = false;

std::vector<std::string> Dir::ResolutionsToEncode;

void FindAndReplaceAll(std::string& data, std::string toSearch, std::string replaceStr)
{
	// Get the first occurrence
	size_t pos = data.find(toSearch);

	// Repeat till end is reached
	while (pos != std::string::npos)
	{
		// Replace this occurrence of Sub String
		data.replace(pos, toSearch.size(), replaceStr);
		// Get the next occurrence from the current position
		pos = data.find(toSearch, pos + replaceStr.size());
	}
}

const std::string CurrentDateTime()
{
	time_t     now = time(0);
	struct tm  tstruct;
	char       buf[80];
	tstruct = *localtime(&now);

	strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

	std::string data(buf);

	FindAndReplaceAll(data, ":", ".");

	return data;
}

const std::string GetAbsoluteDirectory(std::string Directory)
{
	return std::filesystem::absolute(Directory).string();
}

void RenameMultipleFilesToNewDirectory(std::vector<std::string> fileNames, std::string OldDirectory, std::string NewDirectory)
{

}

bool DirectoryExists(std::string FolderPath, bool CreateDirectoryIfDoesNotExist)
{
	namespace fs = std::filesystem;
	std::string directoryName = FolderPath;
	std::error_code ec;

	if (!fs::is_directory(directoryName, ec)) // Check if this is a directory already
	{

		if (!fs::exists(directoryName, ec))//check if folder exists
		{
			if (CreateDirectoryIfDoesNotExist)
			{
				LogFile::WriteToLog("Directory " + directoryName + " created.");
				fs::create_directory(directoryName); // create src folder
				return true;
			}
			else
				return false;
		}
		else
			return false;
	}
	else
		return true;
}

int CreateDirectory(std::string Path)
{
	namespace fs = std::filesystem;
	std::string directoryName = Path;
	std::error_code ec;

	if (!fs::exists(directoryName, ec))//check if folder exists
	{
		EXIT_ON_FS_ERROR(ec);

		
		fs::create_directory(directoryName,ec) EXIT_ON_FS_ERROR(ec); // create src folder
		LogFile::WriteToLog("Directory " + directoryName + " created.");
	}
	
	return 0;
}

void AppendHostName(std::string& data)
{
	TCHAR* infoBuf = new TCHAR[32767];
	DWORD  bufCharCount = 32767;

	// Get and display the name of the computer.
	if (!GetComputerName(infoBuf, &bufCharCount))
	{
		std::cout << "Cannot find computer name" << std::endl;
		LogFile::WriteToLog("Computer name not found.");
	}

	data.append("\nHost: " + std::string(infoBuf) + "\n");
	delete[] infoBuf;
}


bool EnsureSafeExecution(void(*FUNC) (void*, void*, int*), void* data_in, void* data_out)
{
	int retCode = 0;
	do
	{
		try
		{
			ENSURE_DRIVE_SAFETY FUNC(data_in, data_out,&retCode);
		}
		catch (const std::exception&e)
		{
			cout << "CAUGHT EXCEPTION: " << e.what() << endl;
			LogFile::WriteToLog("Exception: " + string(e.what()));
			retCode = -1;
		}

		if (retCode != 0)
		{
			cout << "EnsureSafeExecution - unsuccessful sandbox execution. Retrying in 10 seconds." << endl;
			SLEEP(10000);
		}
		
		LogFile::AddReturnCode(retCode);

		if (LogFile::IsStuckInError())
		{
			retCode = 0;

			LogFile::WriteToLog("Detecting broken infinite loop in EnsureSafeExecution. Attempting reboot of thread.");
			return false;
		}

	} while (retCode != 0);

	return true;
}


bool DrivesAreAccessible()
{
	string outputDrive = Dir::OutputFolder.substr(0, Dir::OutputFolder.find("\\") + 1);
	string sourceDrive = Dir::HotFolder.substr(0, Dir::OutputFolder.find("\\") + 1);

	unsigned int outputRet = GetDriveTypeA(outputDrive.c_str());
	unsigned int sourceRet = GetDriveTypeA(sourceDrive.c_str());

	if (outputRet < 2)
	{
		cout << "Output drive is not mapped." << endl;
		LogFile::WriteToLog("Output drive is not mapped.");
		return false;
	}

	if (sourceRet < 2)
	{
		cout << "Source drive is not mapped." << endl;
		LogFile::WriteToLog("Source drive is not mapped.");
		return false;
	}

	return true;
}

void StripExtraData(string& Directory)
{
	size_t size = Directory.length();
	if (size == 0)
		return;

	char extra = Directory[size - 1];

	if (extra == '/')
		Directory.pop_back();
	else if (extra == '\\')
		Directory.pop_back();
}

void UNSAFE::RunOnceProgramSetup(void* data_in, void* data_out, int* ret)
{
	//check folder arguments and adjust format
	StripExtraData(Dir::HotFolder);
	Dir::HotFolder = GetAbsoluteDirectory(Dir::HotFolder);
	StripExtraData(Dir::OutputFolder);
	Dir::OutputFolder = GetAbsoluteDirectory(Dir::OutputFolder);
	StripExtraData(Dir::EncodeFolder);
	Dir::EncodeFolder = GetAbsoluteDirectory(Dir::EncodeFolder);

	//seed random time
	srand((unsigned int)(time(0)));

	//create an archive folder if it doesnt exist
	string dir = Dir::HotFolder + "\\" + ARCHIVE_DIRECTORY;
	EnsureSafeExecution(CreateDirectoryUnsafe, &dir);
	dir = Dir::HotFolder + "\\" + FAIL_DIRECTORY;
	EnsureSafeExecution(CreateDirectoryUnsafe, &dir);
	dir = Dir::HotFolder + "\\" + ACTIVE_RENDER_DIECTORY;
	EnsureSafeExecution(CreateDirectoryUnsafe, &dir);

	//Make all encoding folders if needed
	for (int i = 0; i < Dir::ResolutionsToEncode.size(); i++)
	{
		std::string subFolder = Dir::ResolutionsToEncode[i];

		//Is this string a number? Add a p if so
		if (!subFolder.empty() && std::all_of(subFolder.begin(), subFolder.end(), ::isdigit))
		{
			subFolder += "p";
		}

		std::string outputDirectory = Dir::EncodeFolder + "\\" + subFolder;

		EnsureSafeExecution(CreateDirectoryUnsafe, &outputDirectory, nullptr);
	}

	*ret = 0;
}

void UNSAFE::FetchProjectBuildLogUnsafe(void* data_in, void* data_out, int* ret)
{
	if (Dir::UsingSqlite)
	{
		try
		{
			Project::SQL_DATA = SQL::SQL_GetProjectBuildLog(Project::OUR_DATABASE, Project::PROJECT_NAME);
		}
		catch (const std::exception & e)
		{
			throw e;
		}
	}
	else
	{
		Project::SQL_DATA = PGSQL::GetProjectBuildLog(Project::PROJECT_NAME, AETL_DB);
	}



	*ret = 0;
}

void UNSAFE::CollectActiveRenderingDataUnsafe(void* data_in, void* data_out, int* ret)
{
	if (Dir::UsingSqlite)
	{
		try
		{
			Project::RENDER_DATA = SQL::SQL_CollectActiveRenderingData(Project::OUR_DATABASE, Project::SQL_DATA);
		}
		catch (const std::exception & e)
		{
			throw e;
		}
	}
	else
	{
		Project::RENDER_DATA = PGSQL::GetActiveRenderingLog(Project::SQL_DATA.Directory, AETL_DB);
	}

	*ret = 0;
}

void UNSAFE::AdjustActiveRenderingDataUnsafe(void* data_in, void* data_out, int* ret)
{
	if (Dir::UsingSqlite)
	{
		try
		{
			SQL::SQL_AdjustActiveRenderInformation(Project::RENDER_DATA, Project::OUR_DATABASE);
		}
		catch (const std::exception & e)
		{
			throw e;
		}
	}
	else
	{
		std::string pSQL = "UPDATE public.\"" + std::string(DATABASE_PROJECT_LOG) + "\" SET \"Retries\"=" + std::to_string(Project::RENDER_DATA.Retries)
			+ ", \"UpdatedAt\"=CURRENT_TIMESTAMP";

		if (Project::RENDER_DATA.Status == "FAILED")
		{
			pSQL += ",\"Status\"='ERROR'";
			pSQL += ",\"VideoRendered\"='0000-00-00 00:00:00'";
		}

		else if (Project::RENDER_DATA.Status == "COMPLETE")
		{
			pSQL += ",\"Status\"='INCOMPLETE'";
			pSQL += ",\"VideoRendered\"=CURRENT_TIMESTAMP";
		}

		pSQL += " WHERE \"Name\"='" + Project::PROJECT_NAME + "';";

		(void)PGSQL::Query(pSQL, AETL_DB);
	}
	*ret = 0;
}

void UNSAFE::AddActiveRenderingDataUnsafe(void* data_in, void* data_out, int* ret)
{
	if (Dir::UsingSqlite)
	{
		try
		{
			Project::RENDER_DATA.CreatedAt = SQL::SQL_AddActiveRenderLog(Project::RENDER_DATA, Project::OUR_DATABASE);
		}
		catch (const std::exception & e)
		{
			throw e;
		}
	}
	else
	{
		(void)PGSQL::Query("UPDATE public.\"" + std::string(DATABASE_PROJECT_LOG) + "\" SET \"Retries\"=" + std::to_string(Project::RENDER_DATA.Retries) + ", \"UpdatedAt\"=CURRENT_TIMESTAMP"
			+ " WHERE \"Name\"='" + Project::PROJECT_NAME + "';", AETL_DB);
	}
	*ret = 0;
}

void UNSAFE::AddRenderLogUnsafe(void* data_in, void* data_out, int* ret)
{
	if (data_in == nullptr)
	{
		LogFile::WriteToLog("AddRenderLog failed. data_in was nullptr");
		*ret = -1;
		return;
	}

	string FolderToCreate = *(string*)(data_in);

	if (Dir::UsingSqlite)
	{
		try
		{
			SQL::SQL_AddRenderLog(Project::SQL_DATA, FolderToCreate, Project::OUR_DATABASE);
		}
		catch (const std::exception & e)
		{
			throw e;
		}
	}
	else
	{
		(void)PGSQL::Query("UPDATE public.\"" + std::string(DATABASE_PROJECT_LOG) + "\" SET \"VideoRendered\"=CURRENT_TIMESTAMP, \"UpdatedAt\"=CURRENT_TIMESTAMP"
			+ " WHERE \"Name\"='" + Project::PROJECT_NAME + "';", AETL_DB);
	}
	
	*ret = 0;
}

void UNSAFE::ObjectExistsUnsafe(void* data_in, void* data_out, int* ret)
{
	bool existVal;

	string* path = (string*)(data_in);
	std::error_code ec;

	existVal = std::filesystem::exists(*path, ec);
	*ret = ec.value() PRINT_FS_ERROR(ec) EXIT_ON_ERROR(*ret);

	//pass the result if they care
	if (data_out != nullptr)
	{
		bool* result = static_cast<bool*>(data_out);
		*result = existVal;
	}

	*ret = 0;
}

void UNSAFE::CreateDirectoryUnsafe(void* data_in, void* data_out, int* ret)
{
	string* path = static_cast<string*>(data_in);

	namespace fs = std::filesystem;

	std::error_code ec;

	if (!fs::exists(*path, ec))//check if folder exists
	{
		*ret = ec.value() PRINT_FS_ERROR(ec) EXIT_ON_ERROR(*ret);

		fs::create_directory(*path, ec); // create src folder
		*ret = ec.value() PRINT_FS_ERROR(ec) EXIT_ON_ERROR(*ret);

		LogFile::WriteToLog("Directory " + *path + " created.");
	}

	*ret = 0;

}

void UNSAFE::RenameFileUnsafe(void* data_in, void* data_out, int* ret)
{
	
	namespace fs = std::filesystem;
	std::error_code ec;

	fs::path* oldFile = static_cast<fs::path*>(data_in);
	fs::path* newFile = static_cast<fs::path*>(data_out);

	LogFile::WriteToLog("Rename: " + oldFile->string() + " to " + newFile->string());

	fs::rename(*oldFile, *newFile, ec);
	*ret = ec.value() PRINT_FS_ERROR(ec) EXIT_ON_ERROR(*ret);

	*ret = 0;
}

void UNSAFE::IsDirectoryUnsafe(void* data_in, void* data_out, int* ret)
{
	std::error_code ec;
	std::filesystem::directory_entry* entry = (std::filesystem::directory_entry*)data_in;


	bool val = std::filesystem::is_directory(*entry, ec);

	*ret = ec.value() PRINT_FS_ERROR(ec) EXIT_ON_ERROR(*ret);

	bool* out = static_cast<bool*>(data_out);
	*out = val;

	*ret = 0;
}

void UNSAFE::RemoveDirectoryUnsafe(void* data_in, void* data_out, int* ret)
{
	if (data_in == nullptr)
	{
		LogFile::WriteToLog("RemoveDirectoryUnsafe failed. data_in was nullptr");
		*ret = -1;
		return;
	}

	namespace fs = std::filesystem;
	std::error_code ec;
	string* DirectoryToRemove = (string*)(data_in);

	fs::remove_all(*DirectoryToRemove, ec);
	*ret = ec.value() PRINT_FS_ERROR(ec) EXIT_ON_ERROR(*ret);
	*ret = 0;
}

void UNSAFE::AviCleanupUnsafe(void* data_in, void* data_out, int* ret)
{
	string* ProjectName = (string*)(data_in);
	bool* SuccessfulRender = (bool*)(data_out);

	string aviFileToFind = Dir::OutputFolder + "\\" + *ProjectName + ".avi";

	LogFile::WriteToLog("Cleaning up AVI file.");

	if (*SuccessfulRender == true)
	{
		std::filesystem::path oldFile = aviFileToFind;
		std::filesystem::path newFile = Dir::EncodeFolder + "\\" + *ProjectName + ".avi";

		LogFile::WriteToLog("Old filepath: " + aviFileToFind);
		LogFile::WriteToLog("New filepath: " + newFile.string())
			;
		EnsureSafeExecution(RenameFileUnsafe, &oldFile, &newFile);
		LogFile::WriteToLog("AVI file " + oldFile.string() + " moved to " + newFile.string() + ".");
	}
	else
	{
		bool result = true;
		EnsureSafeExecution(ObjectExistsUnsafe, &aviFileToFind, &result);

		if (result == true)
			if (remove(aviFileToFind.c_str()) == 0)
			{
				cout << aviFileToFind << " deleted." << endl << endl;
				LogFile::WriteToLog("AVI file " + aviFileToFind + " deleted");
			}
			else
			{
				cout << aviFileToFind << " attempted to delete but failed." << endl << endl;
				LogFile::WriteToLog("AVI file " + aviFileToFind + " failed to be deleted.");
			}
		else
			LogFile::WriteToLog("AVI file not found for deletion.");
	}
}

void UNSAFE::EncodeCleanup(void* data_in, void* data_out, int* ret)
{
	string* VideoLocation = (string*)(data_in);
	bool* SuccessfulRender = (bool*)(data_out);

	bool exists = true;
	EnsureSafeExecution(ObjectExistsUnsafe, VideoLocation, &exists);

	//If the render was successful then rename the file (to unlock it) and leave
	if (*SuccessfulRender)
	{
		std::filesystem::path oldFile = *VideoLocation;
		std::filesystem::path newFile = VideoLocation->substr(0, VideoLocation->find(".lock")) + ".mp4";

		LogFile::WriteToLog("Old filepath: " + oldFile.string());
		LogFile::WriteToLog("New filepath: " + newFile.string())
			;
		EnsureSafeExecution(RenameFileUnsafe, &oldFile, &newFile);
		LogFile::WriteToLog("mp4 file " + oldFile.string() + " moved to " + newFile.string() + ".");
		return;
	}

	//If we didnt successfully render then cleanup the file 
	if (exists == true)
		if (remove(VideoLocation->c_str()) == 0)
		{
			cout << VideoLocation << " deleted." << endl << endl;
			LogFile::WriteToLog("mp4 file " + *VideoLocation + " deleted");
		}
		else
		{
			cout << VideoLocation << " attempted to delete but failed." << endl << endl;
			LogFile::WriteToLog("mp4 file " + *VideoLocation + " failed to be deleted.");
		}
	else
		LogFile::WriteToLog("mp4 file not found for deletion.");
}

void UNSAFE::DeleteAllLockfilesForProject(void* data_in, void* data_out, int* ret)
{
	std::string baseFolder = Dir::EncodeFolder + "\\";

	for (int i = 0; i < Dir::ResolutionsToEncode.size(); i++)
	{
		std::string outputSubdirectory = Dir::ResolutionsToEncode[i];

		//Is this string a number? Add a p if so
		if (!outputSubdirectory.empty() && std::all_of(outputSubdirectory.begin(), outputSubdirectory.end(), ::isdigit))
		{
			outputSubdirectory += "p";
		}

		//Only delete lockfiles since those are likely incomplete. The mp4s can stay
		std::string lockFile = baseFolder + outputSubdirectory + "\\" + Project::PROJECT_NAME + ".lock";

		bool exists = true;
		EnsureSafeExecution(ObjectExistsUnsafe, &lockFile, &exists);

		if (exists)
			if (remove(lockFile.c_str()) == 0)
			{
				cout << lockFile << " deleted." << endl << endl;
				LogFile::WriteToLog("locked file " + lockFile + " deleted");
			}
			else
			{
				cout << lockFile << " attempted to delete but failed." << endl << endl;
				LogFile::WriteToLog("locked file " + lockFile + " failed to be deleted.");
			}
		else
			LogFile::WriteToLog("locked file not found for deletion.");
	}
}

void UNSAFE::GetDirectoryIterator(void* data_in, void* data_out, int* ret)
{
	if (data_in == nullptr)
	{
		LogFile::WriteToLog("GetDirectoryIterator failed. data_in was nullptr");
		*ret = -1;
		return;
	}

	namespace fs = std::filesystem;
	std::error_code ec;

	string* DirectoryToIterate = (string*)(data_in);
	fs::directory_iterator* outIterator = static_cast<fs::directory_iterator*>(data_out);

	*outIterator = fs::directory_iterator(*DirectoryToIterate, ec);
	*ret = ec.value() PRINT_FS_ERROR(ec) EXIT_ON_ERROR(*ret);
	*ret = 0;
}

void UNSAFE::CreateOutputLogUnsafe(void* data_filename, void* data_to_write, int* ret)
{
	if (data_filename == nullptr)
	{
		cout << "CreateOutpoutLogUnsafe failed: data_filename is nullptr" << endl;
		LogFile::WriteToLog("CreateOutpoutLogUnsafe failed: data_filename is nullptr");
		*ret = -1;
		return;
	}

	string* filepath = (string*)(data_filename);
	std::ofstream output(*filepath);

	string* data = (string*)(data_to_write);

	if (output.is_open())
	{
		output << *data << std::endl;
		output.close();
		LogFile::WriteToLog("Output log successfully generated.");
	}
}

void UNSAFE::AttemptVideoRender(void* data_in, void* data_out, int* ret)
{
	if (data_in == nullptr)
	{
		cout << "AttemptVideoRender failed: data_in is nullptr" << endl;
		LogFile::WriteToLog("AttemptVideoRender failed: data_in is nullptr");
		*ret = -1;
		return;
	}

	string* command = (string*)(data_in);

	try
	{
		kGUICallThread ProcessHandle;
		ProcessHandle.Start(command->c_str(), CALLTHREAD_READ);

		//get the render output
		std::string out = *ProcessHandle.GetString();
		cout << out;
		ProcessHandle.Stop();

		string* output = (string*)(data_out);
		*output = out;
	}
	catch (const std::exception&)
	{
		cout << "aerender exception caught. Adobe rendering stopped unexpectedly." << endl;
		LogFile::WriteToLog("aerender exception caught. Adobe rendering stopped unexpectedly.");
	}

	*ret = 0;
}

void UNSAFE::AttemptVideoEncode(void* data_in, void* data_out, int* ret)
{
	if (data_in == nullptr)
	{
		cout << "AttemptVideoEncode failed: data_in is nullptr" << endl;
		LogFile::WriteToLog("AttemptVideoEncode failed: data_in is nullptr");
		*ret = -1;
		return;
	}

	string* command = (string*)(data_in);

	try
	{
		kGUICallThread ProcessHandle;
		ProcessHandle.Start(command->c_str(), CALLTHREAD_READ);

		//get the render output
		std::string out = *ProcessHandle.GetString();
		cout << out;
		ProcessHandle.Stop();

		string* output = (string*)(data_out);
		*output = out;
	}
	catch (const std::exception&)
	{
		cout << "ffmpeg exception caught. ffmpeg stopped unexpectedly." << endl;
		LogFile::WriteToLog("ffmpeg exception caught. ffmpeg stopped unexpectedly.");
	}

	*ret = 0;
}

void UNSAFE::FreeSpaceAvailable(void* data_in, void* data_out, int* ret)
{
	DiskInfo* info = static_cast<DiskInfo*>(data_in);
	string* path = static_cast<string*>(data_out);

	if (info == nullptr)
	{
		cout << "FreeSpaceAvailable failed: info conversion failed" << endl;
		LogFile::WriteToLog("FreeSpaceAvailable failed: info conversion failed");
		*ret = -1;
		return;
	}

	if (path == nullptr)
	{
		cout << "FreeSpaceAvailable failed: path conversion failed" << endl;
		LogFile::WriteToLog("FreeSpaceAvailable failed: path conversion failed");
		*ret = -1;
		return;
	}

	int success = ::GetDiskFreeSpaceEx(path->c_str(),                 // directory name
		&info->FreeBytesAvailable,      // bytes available to caller
		&info->TotalNumberOfBytes,      // bytes on disk
		&info->TotalNumberOfFreeBytes); // free bytes on disk

	if (success == 0)
		*ret = -1 EXIT_ON_ERROR(*ret);
		

	const LONGLONG nGBFactor = 1024 * 1024 * 1024;

	// get free space in GB.
	info->FreeSpaceInGigaBytes = (double)(LONGLONG)info->TotalNumberOfFreeBytes.QuadPart / nGBFactor;
	info->PercentUsed = 100.0 - ((double)(LONGLONG)info->TotalNumberOfFreeBytes.QuadPart / (double)(LONGLONG)info->TotalNumberOfBytes.QuadPart) * 100.0;

	*ret = 0;
}

void UNSAFE::CheckIfDrivesAreAccessible(void* data_in, void* data_out, int* ret)
{
	string outputDrive = Dir::OutputFolder.substr(0, Dir::OutputFolder.find("\\") + 1);
	string sourceDrive = Dir::HotFolder.substr(0, Dir::OutputFolder.find("\\") + 1);

	unsigned int outputRet = GetDriveTypeA(outputDrive.c_str());
	unsigned int sourceRet = GetDriveTypeA(sourceDrive.c_str());

	bool* dataOut = static_cast<bool*>(data_out);

	if (outputRet < 2)
	{
		cout << "Output drive is not mapped." << endl;
		LogFile::WriteToLog("Output drive is not mapped.");
		*dataOut = false;
		*ret = 0;
		return;
	}

	if (sourceRet < 2)
	{
		cout << "Source drive is not mapped." << endl;
		LogFile::WriteToLog("Source drive is not mapped.");
		*dataOut = false;
		*ret = 0;
		return;
	}

	*dataOut = true;
}

void UNSAFE::IsHotFolderLocked(void* data_in, void* data_out, int* ret)
{
	std::error_code ec;
	bool isLocked = filesystem::exists(Dir::HotFolder + "\\" + LISTENER_FILE_NAME, ec);

	//check all files in the directory
	if (isLocked)
	{
		cout << "Hot Folder is currently locked...\n";
		LogFile::WriteToLog("Hot Folder is currently locked.");
	}

	*ret = ec.value() PRINT_FS_ERROR(ec) EXIT_ON_ERROR(*ret);

	bool* out = static_cast<bool*>(data_out);
	*out = isLocked;

	*ret = 0;
}
