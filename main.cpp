#include <thread>
#include <filesystem>
#include <string>
#include <iostream>
#include <windows.h>
#include <conio.h>
#include "kguithread.h"
#include "Helpers.h"
#include "sqllite/SQL_Helpers.h"
#include <fstream>

#define SLEEP(_VAL) std::this_thread::sleep_for(std::chrono::milliseconds(_VAL))
#define RENDER(_PROJECT) "-project \"" + _PROJECT + "\""

//listener file def
#define LISTENER_FILE_NAME string("_DO_NOT_READ_")

//Archive folder def
#define ARCHIVE_DIRECTORY "Archive"

//Active rendering folder ref
#define ACTIVE_RENDER_DIECTORY "Rendering"

//Failed folder def
#define FAIL_DIRECTORY "Failed"

using namespace std;
namespace fs = std::experimental::filesystem;

//Our update loop
void Update();

//Run a command
std::string ExecuteConsoleCommand(std::string AbsEXEPath, std::string Args);

//Execute a process
bool StartProcess(void* pWindow, const char* pOperation, const char* pFile, const char* pParameters, const char* pDirectory);

//Check if we're allowed to work
bool CheckToSeeIfHotFolderIsLocked();

//Remove extra data
void StripExtraData(string & Directory);

//create a log with the data of what happened on an unsuccessful render
void CreateLog(string filepath, string& data);

//Listen for a character input to exit
void ListenForExit();

namespace Dir
{
	string HotFolder = "";//the folder we watch
	string RenderPath = "";//the path to adobe render exe
	string DatabasePath = "";
	string OutputFolder = "";
}

volatile bool complete = false;

sqlite3* OUR_DATABASE;

int main(int argc, char* argv[])
{
	//greeting
	cout << " -- AETL-Listener -- " << endl;
	cout << "Press Escape to initiate shutdown at any time." << endl << endl;

	//there isnt enough args, get out
	if (argc != 9)
	{
		cout << "Not enough arguments to run program. Requires a hot folder and a render path.\n";
		complete = true;
	}

	//load our hot folder from arguments
	for (int i = 0; i < argc; i++)
	{
		if (string(argv[i]) == "-i")
			if (i + 1 < argc)
				Dir::HotFolder = string(argv[i + 1]);
			else
			{
				cout << "-i flag found but no argument." << endl;
				complete = true;
			}
		if (string(argv[i]) == "-ae")
			if (i + 1 < argc)
				Dir::RenderPath = string(argv[i + 1]);
			else
			{
				cout << "-ae flag found but no argument." << endl;
				complete = true;
			}
		if (string(argv[i]) == "-db")
			if (i + 1 < argc)
				Dir::DatabasePath = string(argv[i + 1]);
			else
			{
				cout << "-db flag found but no argument." << endl;
				complete = true;
			}

		if (string(argv[i]) == "-o")
			if (i + 1 < argc)
				Dir::OutputFolder = string(argv[i + 1]);
			else
			{
				cout << "-o flag found but no argument." << endl;
				complete = true;
			}

	}

	if (Dir::HotFolder == "")
	{
		cout << "Hot folder not set properly.\n";
		complete = true;
	}
	if (Dir::RenderPath == "")
	{
		cout << "Adobe Render path not set properly. \n";
		complete = true;
	}

	if (SQL::SQL_LoadDatabase(&OUR_DATABASE, Dir::DatabasePath) == false)
	{
		cout << "Database failed to open" << endl;
		complete = true;
	}

	//do some run-once stuff before looping forever
	if (!complete)
	{
		//check folder arguments and adjust format
		StripExtraData(Dir::HotFolder);
		Dir::HotFolder = GetAbsoluteDirectory(Dir::HotFolder);
		StripExtraData(Dir::OutputFolder);
		Dir::OutputFolder = GetAbsoluteDirectory(Dir::OutputFolder);

		//seed random time
		srand((unsigned int)(time(0)));

		//create an archive folder if it doesnt exist
		DirectoryExists(Dir::HotFolder + "\\" + ARCHIVE_DIRECTORY);
		DirectoryExists(Dir::HotFolder + "\\" + FAIL_DIRECTORY);
		DirectoryExists(Dir::HotFolder + "\\" + ACTIVE_RENDER_DIECTORY);
	}

#if (_DEBUG)
	std::thread InputListener(ListenForExit);
#elif (!_DEBUG)
	std::thread* InputListener = new std::thread(ListenForExit);
	InputListener->detach();
#endif

	while (!complete)
	{
		//only update if the lock isnt present on the folder
		if (CheckToSeeIfHotFolderIsLocked() == false)
			Update();

		//sleep for 1 second + a random amount between 0-10 seconds
		SLEEP(1000 + rand() % 10000);
	}

#if (_DEBUG)
	InputListener.join();
#elif (!_DEBUG)
	delete InputListener;
#endif
	cout << endl << "Program has been shut down." << endl;
	system("PAUSE");
	return 0;
}


void Update()
{
	//find a project file 
	for (const auto& entry : fs::directory_iterator(Dir::HotFolder))
	{
		//our potential project
		fs::path projectPath = entry.path();
		string projectName = projectPath.string();

		//if it's a directory ignore it
		if (fs::is_directory(entry))
			continue;
		//If a non-project file is within, ignore it
		else if (projectName.find(".aep") == string::npos)
			continue;

		SQL::ProjectSQLData projectData = SQL::SQL_GetProjectBuildLog(OUR_DATABASE, projectName);
		string dateUsedByActiveRenderLog;

		//strip data to have ONLY the project name
		FindAndReplaceAll(projectName, ".aep", "");
		FindAndReplaceAll(projectName, Dir::HotFolder + "\\", "");

		//if this happened it means there isnt a log entry for this project... we can still render it but we won't be able to log the data later and the ID might be messed up
		bool addLogToSQLAfterRendering = true;
		if (projectData.ProjectID == "")
		{
			projectData.ProjectID = projectName;
			addLogToSQLAfterRendering = false;
		}
		else
		{
			//Add a log to the active rendering table notifying that we're starting a render
			projectData.Directory = Dir::OutputFolder + "\\" + projectName + ".avi";
			dateUsedByActiveRenderLog = SQL::SQL_AddActiveRenderLog(DATABASE_ACTIVE_LOG, projectData.ProjectID, projectData.ProjectType, projectData.ImageType, projectData.Directory, OUR_DATABASE);
		}

		//Create subfolders for this project if they arent already there
		DirectoryExists(Dir::HotFolder + "\\" + ARCHIVE_DIRECTORY + "\\" + projectData.ProjectID);
		DirectoryExists(Dir::HotFolder + "\\" + ACTIVE_RENDER_DIECTORY + "\\" + projectData.ProjectID);
		DirectoryExists(Dir::HotFolder + "\\" + FAIL_DIRECTORY + "\\" + projectData.ProjectID);

		//Create a stamped filename
		string dateStamp = CurrentDateTime();
		string lockedFileName = projectName + "_" + dateStamp + ".aep";

		//create our new project directory and rename the file
		string currentRenderingDirectory = Dir::HotFolder + "\\" + ACTIVE_RENDER_DIECTORY + "\\" + projectData.ProjectID + "\\" + dateStamp;
		DirectoryExists(currentRenderingDirectory);
		fs::path newProjectPath = fs::absolute(Dir::HotFolder + "\\" + ACTIVE_RENDER_DIECTORY + "\\" + projectData.ProjectID + "\\" + dateStamp + "\\" + lockedFileName);
		fs::rename(projectPath, newProjectPath);
		SLEEP(500);

		//notify the render process
		cout << "Project " << projectName << " now being rendered. Locking....\n\n";

		//render the video
		kGUICallThread ProcessHandle;
		std::string command = Dir::RenderPath + " " + RENDER(newProjectPath.string());
		ProcessHandle.Start(command.c_str(), CALLTHREAD_READ);

		//sleep for a few seconds after exiting to ensure all files are written
		SLEEP(3000);

		//get the render output
		std::string out = *ProcessHandle.GetString();
		cout << out;

		//copy log names to vector to move to a different location
		vector<string> aeLogs;
		for (const auto& logs : fs::directory_iterator(newProjectPath.string() + " Logs"))
		{
			string filename = logs.path().string();
			FindAndReplaceAll(filename, newProjectPath.string(), "");
			FindAndReplaceAll(filename, " Logs\\", "");
			aeLogs.push_back(filename);
		}

		//Our folder we will create to archive everything after
		string FolderToCreate;
		string newActiveData;

		//Render succeeded.
		if (out.find("Finished composition") != string::npos && out.find("Unable to Render:") == string::npos)
		{
			FolderToCreate = Dir::HotFolder + "\\" + ARCHIVE_DIRECTORY + "\\" + projectData.ProjectID + "\\" + dateStamp;
			projectData.Directory = Dir::OutputFolder + "\\" + projectName + ".avi";
			newActiveData = "COMPLETE";

			//Notify user the success is complete.
			cout << "\nProject " << projectName << " rendered successfully.\n\n";
		}
		//the process failed to read and didnt create the video.
		else
		{
			FolderToCreate = Dir::HotFolder + "\\" + FAIL_DIRECTORY + "\\" + projectData.ProjectID + "\\" + dateStamp;
			projectData.Directory = "NULL";
			newActiveData = "FAILED";

			//Notify user the failure is complete.
			cout << "\nProject " << projectName << " failed to render properly.\n\n";
		}

		//Store all the data in either archive or fail based on log info
		DirectoryExists(FolderToCreate);
		fs::rename(newProjectPath, fs::path(FolderToCreate + "\\" + lockedFileName));
		CreateLog(FolderToCreate + "\\" + lockedFileName, out);

		//add our log to the render list and adjust the active render log we input earlier to be finished
		if (addLogToSQLAfterRendering == true)
		{
			SQL::SQL_AddObjectToTable(DATABASE_RENDER_LOG, projectData.ProjectID, projectData.ProjectType, projectData.ImageType, projectData.Directory, FolderToCreate, OUR_DATABASE);
			SQL::SQL_AdjustActiveRenderInformation(DATABASE_ACTIVE_LOG, projectData.ProjectID, projectData.ProjectType, projectData.ImageType, dateUsedByActiveRenderLog, newActiveData, OUR_DATABASE);
		}

		//clean up adobes mess
		RenameMultipleFilesToNewDirectory(aeLogs, currentRenderingDirectory + "\\" + lockedFileName + " Logs", FolderToCreate);
		fs::remove_all(currentRenderingDirectory);

		break;	
	}
}

std::string ExecuteConsoleCommand(std::string AbsEXEPath, std::string Args)
{
	std::string out;
	if (StartProcess(NULL, "open", AbsEXEPath.c_str(), Args.c_str(), NULL) == false)
		out = "Program " + AbsEXEPath + " failed to run with args " + Args;
	else
		out = "Program " + AbsEXEPath + " successfully executed.";

	return out;
}

bool StartProcess(void* pWindow, const char* pOperation, const char* pFile, const char* pParameters, const char* pDirectory)
{
	bool rc = false;

	//Our execute structure
	SHELLEXECUTEINFO ShExecInfo = { 0 };
	ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	ShExecInfo.hwnd = (HWND)pWindow;
	ShExecInfo.lpVerb = NULL;
	ShExecInfo.lpFile = pFile;
	ShExecInfo.lpParameters = pParameters;
	ShExecInfo.lpDirectory = NULL;
	ShExecInfo.nShow = SW_HIDE;
	ShExecInfo.hInstApp = NULL;


	if (ShellExecuteEx(&ShExecInfo))
	{
#pragma warning(suppress : 6387)
		WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
		CloseHandle(ShExecInfo.hProcess);

		rc = true;
	}

	return rc;
}

bool CheckToSeeIfHotFolderIsLocked()
{
	bool isLocked = fs::exists(Dir::HotFolder + "\\" + LISTENER_FILE_NAME);

	//check all files in the directory
	if (isLocked)
		cout << "Hot Folder is currently locked. Will try again in 10 seconds.\n";

	return isLocked;
}

void StripExtraData(string & Directory)
{
	size_t size = Directory.length();
	char extra = Directory[size - 1];

	if (extra == '/')
		Directory.pop_back();
	else if (extra == '\\')
		Directory.pop_back();
}

void ListenForExit()
{
	while (true)
	{
		int val = _getch();

		if (val == 0x1B)
		{
			cout << endl << "Exit has been requested. Finishing up last directive then shutting down." << endl;
			complete = true;
			break;
		}
	}
}
