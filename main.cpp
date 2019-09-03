#include <windows.h>
#include <conio.h>
#include "kguithread.h"
#include "Helpers.h"
#include "sqllite/SQL_Helpers.h"
#include <fstream>
#include "LogFile.h"

#define RENDER(_PROJECT) "-project \"" + _PROJECT + "\""

#define AETL_VERSION "0.9b"

using namespace std;
namespace fs = std::experimental::filesystem;
using namespace UNSAFE;

//Our update loop
void Update();

//Check if we're allowed to work
bool CheckToSeeIfHotFolderIsLocked();

//Listen for a character input to exit
void ListenForExit();

volatile bool complete = false;

int main(int argc, char* argv[])
{
	//greeting
	std::cout << " -- AETL-Listener -- " << endl;
	std::cout << "VERSION: " << AETL_VERSION << endl;
	std::cout << "Press Escape to initiate shutdown at any time." << endl << endl;

	LogFile::BeginLogging();

	//there isnt enough args, get out
	if (argc != 13)
	{
		LogFile::WriteToLog("Argument count not correct. " + to_string(argc) + " found. Expected 11.");
		std::cout << "Argument count not correct.\n";
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
				std::cout << "-i flag found but no argument." << endl;
				complete = true;
			}
		if (string(argv[i]) == "-ae")
			if (i + 1 < argc)
			{
				Dir::RenderPath = string(argv[i + 1]);

				//get our version. If this find fails then the path is messed up anyway so lets get out.
				string adobeVersion = Dir::RenderPath.substr(Dir::RenderPath.find("Adobe After Effects CC "), 27);

				if (adobeVersion == "")
				{
					std::cout << "Adobe path given is incorrect. Expected C:\\Program Files\\Adobe\\Adobe After Effects CC XXXX\\Support Files\\aerender.exe where XXXX is the version." << endl
						<< "Please locate your installation and rerun program with a correct path to it." << endl;
					complete = true;
				}

				Dir::ADOBE_VERSION = atoi(adobeVersion.substr(adobeVersion.size() - 4).c_str());
			}
			else
			{
				std::cout << "-ae flag found but no argument." << endl;
				complete = true;
			}
		if (string(argv[i]) == "-db")
			if (i + 1 < argc)
				Dir::DatabasePath = string(argv[i + 1]);
			else
			{
				std::cout << "-db flag found but no argument." << endl;
				complete = true;
			}

		if (string(argv[i]) == "-o")
			if (i + 1 < argc)
				Dir::OutputFolder = string(argv[i + 1]);
			else
			{
				std::cout << "-o flag found but no argument." << endl;
				complete = true;
			}

		if (string(argv[i]) == "-c")
			if (i + 1 < argc)
				Dir::CopyFolder = string(argv[i + 1]);
			else
			{
				std::cout << "-c flag found but no argument." << endl;
				complete = true;
			}

		if (string(argv[i]) == "-usage")
			if (i + 1 < argc)
				Dir::PercentThreshold = stod(argv[i + 1]);
			else
			{
				std::cout << "-usage flag found but no argument." << endl;
				complete = true;
			}

	}


	if (Dir::HotFolder == "")
	{
		std::cout << "Hot folder not set properly.\n";
		complete = true;
	}
	if (Dir::RenderPath == "")
	{
		std::cout << "Adobe Render path not set properly. \n";
		complete = true;
	}

	if (SQL::SQL_LoadDatabase(&Project::OUR_DATABASE, Dir::DatabasePath) == false)
	{
		std::cout << "Database failed to open" << endl;
		LogFile::WriteToLog("Database failed to open");
		complete = true;
	}
	else
		LogFile::WriteToLog("Database loaded successfully.");

	//do some run-once stuff before looping forever
	if (!complete)
		EnsureSafeExecution(RunOnceProgramSetup);

#if (_DEBUG)
	std::thread InputListener(ListenForExit);
#elif (!_DEBUG)
	std::thread* InputListener = new std::thread(ListenForExit);
	InputListener->detach();
#endif

	LogFile::WriteToLog(string("Running program with these arguments: ")
		+ "\n\t\t\tHot folder: " + Dir::HotFolder
		+ "\n\t\t\tOutput folder: " + Dir::OutputFolder
		+ "\n\t\t\tRenderer: " + Dir::RenderPath
		+ "\n\t\t\tAdobe Version: " + to_string(Dir::ADOBE_VERSION)
		+ "\n\t\t\tMax Percent threshold: " + to_string(Dir::PercentThreshold)
	);


	LogFile::WriteToLog("Beginning runtime loop.");

	//our runtime loop. this will run forever unless exited by pressing the ESCAPE key
	while (!complete)
	{
		//check if the hot folder is locked
		bool isFolderLocked;
		EnsureSafeExecution(UNSAFE::IsHotFolderLocked, nullptr, &isFolderLocked);

		//if the folder isnt locked we can continue
		if (isFolderLocked == false)
		{
			//get free space information
			DiskInfo checkDisk;
			string outputDrive = Dir::OutputFolder.substr(0, Dir::OutputFolder.find("\\") + 1);
			EnsureSafeExecution(UNSAFE::FreeSpaceAvailable, &checkDisk, &outputDrive);

			//if the disk is above usage specified, don't update. wait and try again.
			if (checkDisk.PercentUsed > Dir::PercentThreshold)
			{
				std::cout << "Disk is above capacity. Waiting 1 minute then trying again..." << endl;
				LogFile::WriteToLog("Disk is above capacity. Waiting 1 minute then trying again...");
				SLEEP(60000);
			}
			else
			{

				std::cout << "Disk check successful. Printing disk values:" << endl;
				std::cout << "Total space : " << checkDisk.TotalNumberOfBytes.QuadPart << " bytes" << endl
					<< "Free space : " << checkDisk.FreeBytesAvailable.QuadPart << " bytes" << endl
					<< "Free space in GB : " << checkDisk.FreeSpaceInGigaBytes << " GB" << endl
					<< "Percent used : " << checkDisk.PercentUsed << "%" << endl << endl;

				LogFile::WriteToLog("Successful disk check. Space available in GB: " + to_string(checkDisk.FreeSpaceInGigaBytes) + ". Percent used: " + to_string(checkDisk.PercentUsed) + ".");

				//the runtime loop body
				Update();
			}
		}
	

		//sleep for 1 second + a random amount between 0-60 seconds
		SLEEP(1000 + rand() % 60000);
	}

	LogFile::WriteToLog("Ending runtime loop.");

#if (_DEBUG)
	InputListener.join();
#elif (!_DEBUG)
	delete InputListener;
#endif
	std::cout << endl << "Program has been shut down." << endl;
	LogFile::EndLogging();
	system("PAUSE");
	return 0;
}


void Update()
{

	//safely create a directory iterator
	fs::directory_iterator directoryIterator;
	EnsureSafeExecution(GetDirectoryIterator, &Dir::HotFolder, &directoryIterator);

	//find a project file 
	for (const auto& entry : directoryIterator)
	{
		//our potential project
		fs::path projectPath = entry.path();
		Project::PROJECT_NAME = projectPath.string();

		//if it's a directory ignore it
		bool val = true;
		EnsureSafeExecution(IsDirectoryUnsafe, (void*)& entry, &val);
		if (val == true)
			continue;
		//If a non-project file is within, ignore it
		else if (Project::PROJECT_NAME.find(".aep") == string::npos)
			continue;

		LogFile::WriteToLog("---------- START OF RENDER SEQUENCE ----------");
		LogFile::WriteToLog("Project file found: " + Project::PROJECT_NAME);

		//Get our project data from he build log
		LogFile::WriteToLog("Collecting Project Build Log from SQLite...");
		EnsureSafeExecution(FetchSQLBuildLogUnsafe);
		LogFile::WriteToLog("Project Build Log found.");

		//strip data to have ONLY the project name
		FindAndReplaceAll(Project::PROJECT_NAME, ".aep", "");
		FindAndReplaceAll(Project::PROJECT_NAME, Dir::HotFolder + "\\", "");

		LogFile::WriteToLog("Project data: "
			+ string("\n\t\t\tProjectID - ") + Project::SQL_DATA.ProjectID
			+ string("\n\t\t\tLocationID - ") + Project::SQL_DATA.LocationID
			+ string("\n\t\t\tProjectType - ") + Project::SQL_DATA.ProjectType
			+ string("\n\t\t\tImageType - ") + Project::SQL_DATA.ImageType);

		//if this happened it means there isnt a log entry or something happened, either way get out of here.
		if (Project::SQL_DATA.LocationID == "")
		{
			LogFile::WriteToLog("LocationID is blank. Leaving render sequence.");
			break;
		}

		//Add a log to the active rendering table notifying that we're starting a render
		Project::SQL_DATA.Directory = Dir::OutputFolder + "\\" + Project::PROJECT_NAME + ".avi";
		LogFile::WriteToLog("Setting render directory: " + Project::SQL_DATA.Directory);

		//check to see if this project is a retry (a different node failed rendering this)
		EnsureSafeExecution(CollectActiveRenderingDataUnsafe);

		
		if (Project::RENDER_DATA.Status == "RETRY")
			//it's a retry so adjust the data entry for this project accordingly and notify user
		{
			std::cout << "Project " << Project::PROJECT_NAME << " is re-attempting rendering. \nRetry count: " << Project::RENDER_DATA.Retries << "\nRetries remaining: " << 4 - Project::RENDER_DATA.Retries << endl;
			LogFile::WriteToLog("Project " + Project::PROJECT_NAME + ": attempting retry of render process. Retry count: " 
				+ to_string(Project::RENDER_DATA.Retries) + ". Retries remaining: " + to_string(4 - Project::RENDER_DATA.Retries));
			LogFile::WriteToLog("Project " + Project::PROJECT_NAME + ": Adjusting project data entry - Status from RETRY to RENDERING.");
			Project::RENDER_DATA.Status = "RENDERING";

			//Notify the SQLite database we are attempting a retry
			EnsureSafeExecution(AdjustActiveRenderingDataUnsafe);
		}
		else
			//it's a fresh project. Build the RENDER_DATA using SQL_DATA and create a new entry in the database.
		{
			std::cout << "Project " << Project::PROJECT_NAME << " is a new project. Attempting first render." << endl;
 			LogFile::WriteToLog("Project " + Project::PROJECT_NAME + ": Adding Active Render log to SQLite database.");
			Project::RENDER_DATA.ImageType = Project::SQL_DATA.ImageType;
			Project::RENDER_DATA.LocationID = Project::SQL_DATA.LocationID;
			Project::RENDER_DATA.ProjectID = Project::SQL_DATA.ProjectID;
			Project::RENDER_DATA.ProjectType = Project::SQL_DATA.ProjectType;
			Project::RENDER_DATA.Retries = 0;
			Project::RENDER_DATA.Directory = Project::SQL_DATA.Directory;
			Project::RENDER_DATA.Status = "RENDERING";

			//Add a new entry to the SQLite database
			EnsureSafeExecution(AddActiveRenderingDataUnsafe);
		}

		//Create subfolders for this project if they arent already there
		LogFile::WriteToLog("Creating project subfolders.");
		{
			string dir = Dir::HotFolder + "\\" + ARCHIVE_DIRECTORY + "\\" + Project::SQL_DATA.LocationID;
			EnsureSafeExecution(CreateDirectoryUnsafe, &dir);
			dir = Dir::HotFolder + "\\" + ACTIVE_RENDER_DIECTORY + "\\" + Project::SQL_DATA.LocationID;
			EnsureSafeExecution(CreateDirectoryUnsafe, &dir);
			dir = Dir::HotFolder + "\\" + FAIL_DIRECTORY + "\\" + Project::SQL_DATA.LocationID;
			EnsureSafeExecution(CreateDirectoryUnsafe, &dir);
		}

		//Create a stamped filename
		string dateStamp = CurrentDateTime();
		LogFile::WriteToLog("Datestamp used for this render: " + dateStamp);
		string lockedFileName = Project::PROJECT_NAME + "_" + dateStamp + ".aep";
		LogFile::WriteToLog("Locked filename: " + lockedFileName);

		//create our new project directory and rename the file
		string currentRenderingDirectory = Dir::HotFolder + "\\" + ACTIVE_RENDER_DIECTORY + "\\" + Project::SQL_DATA.LocationID + "\\" + dateStamp;
		LogFile::WriteToLog("Creating directory : " + currentRenderingDirectory);
		EnsureSafeExecution(CreateDirectoryUnsafe, &currentRenderingDirectory);
		fs::path newProjectPath = fs::absolute(Dir::HotFolder + "\\" + ACTIVE_RENDER_DIECTORY + "\\" + Project::SQL_DATA.LocationID + "\\" + dateStamp + "\\" + lockedFileName);
		LogFile::WriteToLog("Moving project file from " + projectPath.string() + " to " + newProjectPath.string());
		EnsureSafeExecution(RenameFileUnsafe, &projectPath, &newProjectPath);
		
		//notify the render process
		std::cout << "Project " << Project::PROJECT_NAME << " now being rendered. Locking....\n\n";
		LogFile::WriteToLog("Project " + Project::PROJECT_NAME + " is now being rendered.");

		//render the video
		std::string command = Dir::RenderPath + " " + RENDER(newProjectPath.string());
		std::string out;
		LogFile::WriteToLog("Executing command: " + command);
		EnsureSafeExecution(AttemptVideoRender, &command, &out);

		//sleep for a few seconds after exiting to ensure all files are written
		SLEEP(3000);
		LogFile::WriteToLog("Rendering completed.");

		//copy log names to vector to move to a different location
		vector<string> aeLogs;
		if (Dir::ADOBE_VERSION > 2018)
		{
			LogFile::WriteToLog("Copying AE log names to be moved later.");
			{
				fs::directory_iterator it;
				string path = newProjectPath.string() + " Logs";
				EnsureSafeExecution(GetDirectoryIterator, &path, &it);
				for (const auto& logs : it)
				{
					string filename = logs.path().string();
					FindAndReplaceAll(filename, newProjectPath.string(), "");
					FindAndReplaceAll(filename, " Logs\\", "");
					aeLogs.push_back(filename);
				}
			}
		}
		//Our folder we will create to archive everything after
		string FolderToCreate;

		//Render succeeded.
		bool renderResult = out.find("Finished composition") != string::npos && out.find("Unable to Render:") == string::npos;
		if (renderResult == true)
		{
			LogFile::WriteToLog("Render successful.");
			FolderToCreate = Dir::HotFolder + "\\" + ARCHIVE_DIRECTORY + "\\" + Project::SQL_DATA.LocationID + "\\" + dateStamp;
			Project::SQL_DATA.Directory = Dir::OutputFolder + "\\" + Project::PROJECT_NAME + ".avi";
			Project::RENDER_DATA.Status = "COMPLETE";

			//Notify user the success is complete.
			std::cout << "\nProject " << Project::PROJECT_NAME << " rendered successfully.\n\n";

			//Move the AVI file to the encoder
			LogFile::WriteToLog("Moving AVI file to AETL-Encoder hot folder...");
			EnsureSafeExecution(AviCleanupUnsafe, &Project::PROJECT_NAME, &renderResult);


		}
		//the process failed to read and didnt create the video.
		else
		{
			LogFile::WriteToLog("Render unsuccessful. Check the render output log for more information.");
			FolderToCreate = Dir::HotFolder + "\\" + FAIL_DIRECTORY + "\\" + Project::SQL_DATA.LocationID + "\\" + dateStamp;
			Project::SQL_DATA.Directory = "NULL";

			if (Project::RENDER_DATA.Retries >= 3)
				Project::RENDER_DATA.Status = "FAILED";
			else
			{
				Project::RENDER_DATA.Status = "RETRY";
				Project::RENDER_DATA.Retries++;
			}

			//Notify user the failure is complete.
			std::cout << "\nProject " << Project::PROJECT_NAME << " failed to render properly.\n\n";

			//Delete the AVI file if there is anything to cleanup
			LogFile::WriteToLog("Deleting AVI file...");
			EnsureSafeExecution(AviCleanupUnsafe, &Project::PROJECT_NAME, &renderResult);
		}

		Project::RENDER_DATA.Directory = Project::SQL_DATA.Directory;
		//Add host name to the output log
		LogFile::WriteToLog("Adding host name to output log.");
		AppendHostName(out);

		//Store all the data in either archive or fail based on log info
		if (Project::RENDER_DATA.Status != "RETRY")
		{
			LogFile::WriteToLog("Creating post-render folder " + FolderToCreate);
			EnsureSafeExecution(CreateDirectoryUnsafe, &FolderToCreate);
			LogFile::WriteToLog("Moving project file to post-render folder.");
			fs::path returnPath = fs::path(FolderToCreate + "\\" + lockedFileName);

			EnsureSafeExecution(RenameFileUnsafe,&newProjectPath, &returnPath);
			LogFile::WriteToLog("Creating output log.");
			string outputLogDirectory = FolderToCreate + "\\" + lockedFileName;
			EnsureSafeExecution(CreateOutputLogUnsafe, &outputLogDirectory, &out);
			LogFile::WriteToLog("Output log created.");

			//clean up adobes mess
			LogFile::WriteToLog("Moving Adobe logs to post-render folder (ONLY FOR VERSIONS 2019 OR LATER).");
			for (unsigned int i = 0; i < aeLogs.size(); i++)
			{
				std::experimental::filesystem::path oldFile = currentRenderingDirectory + "\\" + lockedFileName + " Logs" + "\\" + aeLogs[i];
				std::experimental::filesystem::path newFile = FolderToCreate + "\\" + aeLogs[i];
				LogFile::WriteToLog("Renaming " + oldFile.string() + " to " + newFile.string());
				EnsureSafeExecution(RenameFileUnsafe, &oldFile, &newFile);
			}

			//add our log to the render list and adjust the active render log we input earlier to be finished
			LogFile::WriteToLog("Adding SQLite Database Completed Render log.");
			EnsureSafeExecution(AddRenderLogUnsafe, &FolderToCreate);
		}
		//if this is a retry then move the project file back into the project hot folder
		else
		{
			LogFile::WriteToLog("Moving project to hot folder for retry.");
			fs::path backToHotFolder = fs::path(Dir::HotFolder + "\\" + Project::PROJECT_NAME + ".aep");
			EnsureSafeExecution(RenameFileUnsafe, &newProjectPath, &backToHotFolder);
		}

		//adjust our active rendering log
		LogFile::WriteToLog("Adjusting SQLite Database Active Rendering log.");
		EnsureSafeExecution(AdjustActiveRenderingDataUnsafe);

		//cleanup the working directory
		LogFile::WriteToLog("Removing rendering directory and anything within.");
		EnsureSafeExecution(RemoveDirectoryUnsafe, &currentRenderingDirectory);
		LogFile::WriteToLog("---------- END OF RENDER SEQUENCE ----------");
		break;	
	}
}

bool CheckToSeeIfHotFolderIsLocked()
{
	bool isLocked = fs::exists(Dir::HotFolder + "\\" + LISTENER_FILE_NAME);

	//check all files in the directory
	if (isLocked)
	{
		std::cout << "Hot Folder is currently locked...\n";
		LogFile::WriteToLog("Hot Folder is currently locked.");
	}

	return isLocked;
}

void ListenForExit()
{
	while (true)
	{
		int val = _getch();

		if (val == 0x1B)
		{
			std::cout << endl << "Exit has been requested. Finishing up last directive then shutting down." << endl;
			LogFile::WriteToLog("Exit has been requested. Finishing up last directive then shutting down.");
			complete = true;
			break;
		}
	}
}


