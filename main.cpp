#include <thread>
#include <filesystem>
#include <string>
#include <iostream>
#include <windows.h>
#include "kguithread.h"

#define SLEEP(_VAL) std::this_thread::sleep_for(std::chrono::milliseconds(_VAL))
#define RENDER(_PROJECT) "-project \"" + _PROJECT + "\""

//listener file def
#define LISTENER_FILE_NAME string("_DO_NOT_READ_")


using namespace std;
namespace fs = std::experimental::filesystem;

void Update();
void FindAndReplaceAll(std::string& data, std::string toSearch, std::string replaceStr);
const std::string CurrentDateTime();
std::string ExecuteConsoleCommand(std::string AbsEXEPath, std::string Args);
bool StartProcess(void* pWindow, const char* pOperation, const char* pFile, const char* pParameters, const char* pDirectory);

namespace Dir
{
	string HotFolder = "";//the folder we watch
	string RenderPath = "";//the path to adobe render exe
}

int main(int argc, char* argv[])
{

	bool complete = false;

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
		if (string(argv[i]) == "-ae")
			if (i + 1 < argc)
				Dir::RenderPath = string(argv[i + 1]);
	}

	if (Dir::HotFolder == "")
	{
		cout << "Hot folder not set.\n";
		complete = true;
	}
	if (Dir::RenderPath == "")
	{
		cout << "Adobe Render path not set. \n";
		complete = true;
	}

	//seed random
	srand((unsigned int)(time(0)));

	while (!complete)
	{
		Update();

		//sleep for 1 second + a random amount between 0-10 seconds
		SLEEP(1000 + rand() % 10000);
	}

	return 0;
}


void Update()
{
	//Check if the lockfile is active. don't do shit until its gone.
	bool isLocked = true;
	while (isLocked)
	{
		bool stayLocked = false;

		//check all files in the directory
		for (const auto& entry : fs::directory_iterator(Dir::HotFolder))
		{
			//our image path
			fs::path ImagePath = entry.path();

			//strip the data to have ONLY the file name with extension
			string imageNameWithExtension = ImagePath.string();
			FindAndReplaceAll(imageNameWithExtension, Dir::HotFolder + "\\", "");

			//if the lockfile has been found then do not unlock the process loop.
			if (imageNameWithExtension == LISTENER_FILE_NAME)
			{
				cout << "Hot Folder is currently locked. Will try again in 10 seconds.\n";
				stayLocked = true;
			}
		}

		isLocked = stayLocked;

		if (isLocked)
			SLEEP(10000);//wait 10 seconds and check again.
	}

	//find a project file 
	for (const auto& entry : fs::directory_iterator(Dir::HotFolder))
	{
		//our image path
		fs::path ImagePath = entry.path();

		//strip the data to have ONLY the file name with extension
		string imageNameWithExtension = ImagePath.string();
		FindAndReplaceAll(imageNameWithExtension, Dir::HotFolder + "\\", "");

		//strip data to have ONLY the file name without extension
		string imageNameNoExtension = imageNameWithExtension;
		FindAndReplaceAll(imageNameNoExtension, ".aep", "");

		//if we didn't remove the ".aep" tag then it isnt a project file so ignore.
		if (imageNameNoExtension == imageNameWithExtension)
			continue;

		//check if the file is locked, if it isnt locked, lock it ourselves and use it.
		size_t pos = imageNameWithExtension.find("_LOCK");
		if (pos == std::string::npos)
		{
			imageNameNoExtension.append("_" + CurrentDateTime() + "_LOCK.aep");

			//create our new image path
			fs::path NewImagePath = ImagePath;
			NewImagePath.replace_filename(imageNameNoExtension);

			//rename the file
			fs::rename(ImagePath, NewImagePath);

			//notify the render process
			cout << "Project " << imageNameWithExtension << " now being rendered. Locking....\n\n";

			//render the video
			kGUICallThread ProcessHandle;
			std::string command = Dir::RenderPath + " " + RENDER(NewImagePath.string());
			ProcessHandle.Start(command.c_str(), CALLTHREAD_READ);

			std::string out =  *ProcessHandle.GetString();
			cout << out;

			//the process failed to read and didnt create the video, unlock the file.
			if (out.find("LoadLibrary \"n\" failed!"))
			{
				cout << "\nProject " << imageNameWithExtension << " failed to render properly. Unlocking...\n\n";
				fs::rename(NewImagePath, ImagePath);
			}
			else
			{
				cout << "\nProject " << imageNameWithExtension << " rendered successfully.\n\n";
			}


			break;
		}
	}
}

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
		WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
		CloseHandle(ShExecInfo.hProcess);

		rc = true;
	}

	return rc;
}
