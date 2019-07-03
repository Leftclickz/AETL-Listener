#include <thread>
#include <filesystem>
#include <string>
#include <iostream>
#include <windows.h>
#include <conio.h>
#include "kguithread.h"
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
#define FAIL_FILE ".log"

using namespace std;
namespace fs = std::experimental::filesystem;

//Our update loop
void Update();

//Replace an occurence with something else
void FindAndReplaceAll(std::string& data, std::string toSearch, std::string replaceStr);

//Get a datetime stamp
const std::string CurrentDateTime();

//Run a command
std::string ExecuteConsoleCommand(std::string AbsEXEPath, std::string Args);

//Execute a process
bool StartProcess(void* pWindow, const char* pOperation, const char* pFile, const char* pParameters, const char* pDirectory);

//Check if we're allowed to work
bool CheckToSeeIfHotFolderIsLocked();

//Remove extra data
void StripExtraData(string & Directory);

//Check if a directory exists. Can create it if it doesn't exist.
bool DirectoryExists(std::string FolderPath, bool CreateDirectoryIfDoesNotExist = true);

//get our directory
inline string GetAbsoluteDirectory(string Directory)
{
	return fs::absolute(Directory).string();
}

//create a log with the data of what happened on an unsuccessful render
void CreateLog(string filepath, string& data);

//Listen for a character input to exit
void ListenForExit();

//Import a bunch of files
void RenameMultipleFilesToNewDirectory(vector<string> fileNames, string OldDirectory, string NewDirectory);

namespace Dir
{
	string HotFolder = "";//the folder we watch
	string RenderPath = "";//the path to adobe render exe
}

volatile bool complete = false;

int main(int argc, char* argv[])
{
	//greeting
	cout << " -- AETL-Listener -- " << endl;
	cout << "Press Escape to initiate shutdown at any time." << endl << endl;

	//there isnt enough args, get out
	if (argc != 5)
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

	//do some run-once stuff before looping forever
	if (!complete)
	{
		//check folder arguments and adjust format
		StripExtraData(Dir::HotFolder);
		Dir::HotFolder = GetAbsoluteDirectory(Dir::HotFolder);

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

		//strip data to have ONLY the project name
		FindAndReplaceAll(projectName, ".aep", "");
		FindAndReplaceAll(projectName, Dir::HotFolder + "\\", "");

		//Create an archive folder if it exists
		DirectoryExists(Dir::HotFolder + "\\" + ARCHIVE_DIRECTORY + "\\" + projectName);
		DirectoryExists(Dir::HotFolder + "\\" + ACTIVE_RENDER_DIECTORY + "\\" + projectName);
		DirectoryExists(Dir::HotFolder + "\\" + FAIL_DIRECTORY + "\\" + projectName);

		//Create a stamped filename
		string dateStamp = CurrentDateTime();
		string lockedFileName = projectName + "_" + dateStamp + ".aep";

		//create our new project directory and rename the file
		string currentRenderingDirectory = Dir::HotFolder + "\\" + ACTIVE_RENDER_DIECTORY + "\\" + projectName + "\\" + dateStamp;
		DirectoryExists(currentRenderingDirectory);
		fs::path newProjectPath = fs::absolute(Dir::HotFolder + "\\" + ACTIVE_RENDER_DIECTORY + "\\" + projectName + "\\" + dateStamp + "\\" + lockedFileName);
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

		//Render succeeded.
		if (out.find("Finished composition") != string::npos && out.find("Unable to Render:") == string::npos)
		{
			FolderToCreate = Dir::HotFolder + "\\" + ARCHIVE_DIRECTORY + "\\" + projectName + "\\" + dateStamp;

			//Notify user the success is complete.
			cout << "\nProject " << projectName << " rendered successfully.\n\n";
		}
		//the process failed to read and didnt create the video.
		else
		{
			FolderToCreate = Dir::HotFolder + "\\" + FAIL_DIRECTORY + "\\" + projectName + "\\" + dateStamp;

			//Notify user the failure is complete.
			cout << "\nProject " << projectName << " failed to render properly.\n\n";
		}

		//Store all the data in either archive or fail based on log info
		DirectoryExists(FolderToCreate);
		fs::rename(newProjectPath, fs::path(FolderToCreate + "\\" + lockedFileName));
		CreateLog(FolderToCreate + "\\" + lockedFileName, out);

		//clean up adobes mess
		RenameMultipleFilesToNewDirectory(aeLogs, currentRenderingDirectory + "\\" + lockedFileName + " Logs", FolderToCreate);
		fs::remove_all(currentRenderingDirectory);

		break;	
	}
}

void FindAndReplaceAll(std::string & data, std::string toSearch, std::string replaceStr)
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

bool DirectoryExists(std::string FolderPath, bool CreateDirectoryIfDoesNotExist)
{
	namespace fs = std::experimental::filesystem;
	std::string directoryName = FolderPath;

	if (!fs::is_directory(directoryName) || !fs::exists(directoryName)) // Check if src folder exists
		if (CreateDirectoryIfDoesNotExist)
		{
			fs::create_directory(directoryName); // create src folder
			return true;
		}
		else
			return false;
	else
		return true;
}

void CreateLog(string filepath, string & data)
{
	ofstream output(filepath + FAIL_FILE);

	if (output.is_open())
	{
		output << data << endl;
		output.close();
	}
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

void RenameMultipleFilesToNewDirectory(vector<string> fileNames, string OldDirectory, string NewDirectory)
{
	for (unsigned int i = 0; i < fileNames.size(); i++)
	{
		fs::path oldFile = OldDirectory + "\\" + fileNames[i];
		fs::path newFile = NewDirectory + "\\" + fileNames[i];

		fs::rename(oldFile, newFile);
	}
}
