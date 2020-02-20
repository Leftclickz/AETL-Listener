#include <windows.h>
#include <conio.h>
#include "kguithread.h"
#include "Helpers.h"
#include "sqllite/SQL_Helpers.h"
#include <fstream>
#include "LogFile.h"

#define RENDER(_PROJECT) "-project \"" + _PROJECT + "\""

#define TERMINATE_IF_FAILURE(_FUNC_, _VALONE_, _VALTWO_, _ERR_) if (EnsureSafeExecution(_FUNC_, _VALONE_, _VALTWO_) == false) { LoopExecutionComplete = true; LastThreadRenderProgress = _ERR_;LastThreadExecution = LogFile::ERROR_STUCK_IN_ERROR; return;}

#define AETL_VERSION "0.9b"

using namespace std;
namespace fs = std::filesystem;
using namespace UNSAFE;

//Our update loop
void VideoRenderUpdateLoop();

//Check if we're allowed to work
bool CheckToSeeIfHotFolderIsLocked();

//Listen for a character input to exit
void ListenForExit();

//cleanup in case a render fails
void CleanupRenderMess();

volatile bool ProgramExecutionComplete = false;
volatile bool LoopExecutionComplete = false;

LogFile::eThreadExecutionCodes LastThreadExecution;
LogFile::eProgressOfRender LastThreadRenderProgress;

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
		LogFile::WriteToLog("Argument count not correct. " + to_string(argc) + " found. Expected 13.");
		std::cout << "Argument count not correct.\n";
		ProgramExecutionComplete = true;
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
				ProgramExecutionComplete = true;
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
					ProgramExecutionComplete = true;
				}

				Dir::ADOBE_VERSION = atoi(adobeVersion.substr(adobeVersion.size() - 4).c_str());
			}
			else
			{
				std::cout << "-ae flag found but no argument." << endl;
				ProgramExecutionComplete = true;
			}
		if (string(argv[i]) == "-db")
			if (i + 1 < argc)
				Dir::DatabasePath = string(argv[i + 1]);
			else
			{
				std::cout << "-db flag found but no argument." << endl;
				ProgramExecutionComplete = true;
			}

		if (string(argv[i]) == "-o")
			if (i + 1 < argc)
				Dir::OutputFolder = string(argv[i + 1]);
			else
			{
				std::cout << "-o flag found but no argument." << endl;
				ProgramExecutionComplete = true;
			}

		if (string(argv[i]) == "-c")
			if (i + 1 < argc)
				Dir::CopyFolder = string(argv[i + 1]);
			else
			{
				std::cout << "-c flag found but no argument." << endl;
				ProgramExecutionComplete = true;
			}

		if (string(argv[i]) == "-usage")
			if (i + 1 < argc)
				Dir::PercentThreshold = stod(argv[i + 1]);
			else
			{
				std::cout << "-usage flag found but no argument." << endl;
				ProgramExecutionComplete = true;
			}

	}


	if (Dir::HotFolder == "")
	{
		std::cout << "Hot folder not set properly.\n";
		ProgramExecutionComplete = true;
	}
	if (Dir::RenderPath == "")
	{
		std::cout << "Adobe Render path not set properly. \n";
		ProgramExecutionComplete = true;
	}

	if (SQL::SQL_LoadDatabase(&Project::OUR_DATABASE, Dir::DatabasePath) == false)
	{
		std::cout << "Database failed to open" << endl;
		LogFile::WriteToLog("Database failed to open");
		ProgramExecutionComplete = true;
	}
	else
		LogFile::WriteToLog("Database loaded successfully.");

	//do some run-once stuff before looping forever
	if (!ProgramExecutionComplete)
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
	while (!ProgramExecutionComplete)
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

#if (_DEBUG)
				std::thread VideoRenderUpdateThread(VideoRenderUpdateLoop);
#elif (!_DEBUG)
				std::thread* VideoRenderUpdateThread = new std::thread(VideoRenderUpdateLoop);
#endif			

				//Wait for the render process to complete.
				while (LoopExecutionComplete == false)
				{
					SLEEP(1000 + rand() % 60000);
				}
#if (_DEBUG)
				VideoRenderUpdateThread.join();
#elif (!_DEBUG)
				VideoRenderUpdateThread->join();
				delete VideoRenderUpdateThread;
#endif

				LogFile::WriteToLog("Loop execution complete.");
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


void VideoRenderUpdateLoop()
{
	//we need to check if the project has existing data. this only happens if it fails to reset which could be due to a failed render. We'll cleanup best we can and then leave this loop.
	if (Project::PROJECT_NAME != "")
	{
		cout << "Running cleanup process." << endl;
		LogFile::WriteToLog("Running Cleanup.");
		CleanupRenderMess();
		cout << "Cleanup complete." << endl;
		LogFile::WriteToLog("Cleanup complete.");

		Project::Reset();
		return;
	}
	//otherwise we fetch a file. 
	else
	{
		//create a directory iterator
		fs::directory_iterator directoryIterator;
		TERMINATE_IF_FAILURE(GetDirectoryIterator, &Dir::HotFolder, &directoryIterator, LogFile::PRE_RENDER);

		//find a project file 
		for (const auto& entry : directoryIterator)
		{
			//our potential project
			fs::path projectPath = entry.path();
			Project::PROJECT_NAME = projectPath.string();

			//Screen out any non-project files. Basically, if its a directory or a file that does not have a .aep extension.
			{
				bool val = true;
				TERMINATE_IF_FAILURE(IsDirectoryUnsafe, (void*)&entry, &val, LogFile::PRE_RENDER);
				if (val == true)
				{
					Project::PROJECT_NAME = "";
				}
				else if (Project::PROJECT_NAME.find(".aep") == string::npos)
				{
					Project::PROJECT_NAME = "";
				}
			}


			//Keep looking if no file was acquired
			if (Project::PROJECT_NAME == "")
			{
				continue;
			}

			LogFile::WriteToLog("Project file found: " + Project::PROJECT_NAME);
			break;
		}

		//Exit it if there were no project files
		if (Project::PROJECT_NAME == "")
		{
			return;
		}


		LogFile::WriteToLog("---------- START OF RENDER SEQUENCE ----------");

		//Get our project data from the build log
		LogFile::WriteToLog("Collecting Project Build Log from SQLite...");
		TERMINATE_IF_FAILURE(FetchSQLBuildLogUnsafe, nullptr, nullptr, LogFile::PRE_RENDER);
		LogFile::WriteToLog("Project Build Log found.");

		LogFile::WriteToLog("Project data: "
			+ string("\n\t\t\tProjectID - ") + Project::SQL_DATA.ProjectID
			+ string("\n\t\t\tLocationID - ") + Project::SQL_DATA.LocationID
			+ string("\n\t\t\tProjectType - ") + Project::SQL_DATA.ProjectType
			+ string("\n\t\t\tImageType - ") + Project::SQL_DATA.ImageType);

		//if this happened it means there isnt a log entry or something happened, either way get out of here and dont worry about cleanup since we don't care to salvage this.
		if (Project::SQL_DATA.LocationID == "")
		{
			LogFile::WriteToLog("LocationID is blank. Leaving render sequence.");
			return;
		}

		//strip data to have ONLY the project name and store the old path in a temp path
		fs::path projectPath = fs::path(Project::PROJECT_NAME);
		FindAndReplaceAll(Project::PROJECT_NAME, ".aep", "");
		FindAndReplaceAll(Project::PROJECT_NAME, Dir::HotFolder + "\\", "");

		//Create a stamped filename
		Project::TIMESTAMP = CurrentDateTime();
		LogFile::WriteToLog("Timestamp used for this render: " + Project::TIMESTAMP);
		Project::TIMESTAMPED_FILENAME = Project::PROJECT_NAME + "_" + Project::TIMESTAMP + ".aep";
		LogFile::WriteToLog("Locked filename: " + Project::TIMESTAMPED_FILENAME);

		//Create sub folders for this project if they arent already there
		LogFile::WriteToLog("Creating project sub folders.");
		{
			string dir = Dir::HotFolder + "\\" + ARCHIVE_DIRECTORY + "\\" + Project::SQL_DATA.LocationID;
			TERMINATE_IF_FAILURE(CreateDirectoryUnsafe, &dir, nullptr, LogFile::PRE_RENDER);
			dir = Dir::HotFolder + "\\" + ACTIVE_RENDER_DIECTORY + "\\" + Project::SQL_DATA.LocationID;
			TERMINATE_IF_FAILURE(CreateDirectoryUnsafe, &dir, nullptr, LogFile::PRE_RENDER);
			dir = Dir::HotFolder + "\\" + FAIL_DIRECTORY + "\\" + Project::SQL_DATA.LocationID;
			TERMINATE_IF_FAILURE(CreateDirectoryUnsafe, &dir, nullptr, LogFile::PRE_RENDER);
		}

		//create our new project directory and rename the file
		{
			string currentRenderingDirectory = Dir::HotFolder + "\\" + ACTIVE_RENDER_DIECTORY + "\\" + Project::SQL_DATA.LocationID + "\\" + Project::TIMESTAMP;
			LogFile::WriteToLog("Creating directory : " + currentRenderingDirectory);
			TERMINATE_IF_FAILURE(CreateDirectoryUnsafe, &currentRenderingDirectory, nullptr, LogFile::PRE_RENDER);
			Project::CURRENT_RENDERING_DIRECTORY = currentRenderingDirectory;
		}

		//Attempt to move the project to a controlled active rendering folder that is unique. Update the CURRENT_PROJECT_PATH if successful.
		fs::path attemptToMoveProjectPath = fs::absolute(Dir::HotFolder + "\\" + ACTIVE_RENDER_DIECTORY + "\\" + Project::SQL_DATA.LocationID + "\\" + Project::TIMESTAMP + "\\" + Project::TIMESTAMPED_FILENAME);
		LogFile::WriteToLog("Moving project file to " + attemptToMoveProjectPath.string());
		TERMINATE_IF_FAILURE(RenameFileUnsafe, &projectPath, &attemptToMoveProjectPath, LogFile::PRE_RENDER);
		Project::CURRENT_PROJECT_PATH = attemptToMoveProjectPath;

		//Add a log to the active rendering table notifying that we're starting a render
		Project::SQL_DATA.Directory = Dir::OutputFolder + "\\" + Project::PROJECT_NAME + ".avi";
		LogFile::WriteToLog("Setting render directory: " + Project::SQL_DATA.Directory);


		//check to see if this project is a retry (a different node failed rendering this)
		{
			TERMINATE_IF_FAILURE(CollectActiveRenderingDataUnsafe, nullptr, nullptr, LogFile::ATTACHED_PROJECT);

			if (Project::RENDER_DATA.Status == "RETRY")
				//it's a retry so adjust the data entry for this project accordingly and notify user
			{
				std::cout << "Project " << Project::PROJECT_NAME << " is re-attempting rendering. \nRetry count: " << Project::RENDER_DATA.Retries << "\nRetries remaining: " << 4 - Project::RENDER_DATA.Retries << endl;
				LogFile::WriteToLog("Project " + Project::PROJECT_NAME + ": attempting retry of render process. Retry count: "
					+ to_string(Project::RENDER_DATA.Retries) + ". Retries remaining: " + to_string(4 - Project::RENDER_DATA.Retries));
				LogFile::WriteToLog("Project " + Project::PROJECT_NAME + ": Adjusting project data entry - Status from RETRY to RENDERING.");
				Project::RENDER_DATA.Status = "RENDERING";

				//Notify the SQLite database we are attempting a retry
				TERMINATE_IF_FAILURE(AdjustActiveRenderingDataUnsafe, nullptr, nullptr, LogFile::ATTACHED_PROJECT);
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
				TERMINATE_IF_FAILURE(AddActiveRenderingDataUnsafe, nullptr, nullptr, LogFile::ATTACHED_PROJECT);
			}
		}

		//notify the render process
		std::cout << "Project " << Project::PROJECT_NAME << " now being rendered. Locking....\n\n";
		LogFile::WriteToLog("Project " + Project::PROJECT_NAME + " is now being rendered.");

		//render the video
		std::string command = Dir::RenderPath + " " + RENDER(Project::CURRENT_PROJECT_PATH.string());
		std::string out;
		LogFile::WriteToLog("Executing command: " + command);
		TERMINATE_IF_FAILURE(AttemptVideoRender, &command, &out, LogFile::DURING_RENDER);

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
				string path = Project::CURRENT_PROJECT_PATH.string() + " Logs";
				TERMINATE_IF_FAILURE(GetDirectoryIterator, &path, &it, LogFile::POST_RENDER);
				for (const auto& logs : it)
				{
					string filename = logs.path().string();
					FindAndReplaceAll(filename, Project::CURRENT_PROJECT_PATH.string(), "");
					FindAndReplaceAll(filename, " Logs\\", "");
					aeLogs.push_back(filename);
				}
			}
		}

		//Render succeeded.
		bool renderResult = out.find("Finished composition") != string::npos && out.find("Unable to Render:") == string::npos;
		if (renderResult == true)
		{
			LogFile::WriteToLog("Render successful.");
			Project::FINAL_RENDER_FILEPATH = Dir::HotFolder + "\\" + ARCHIVE_DIRECTORY + "\\" + Project::SQL_DATA.LocationID + "\\" + Project::TIMESTAMP;
			Project::SQL_DATA.Directory = Dir::OutputFolder + "\\" + Project::PROJECT_NAME + ".avi";
			Project::RENDER_DATA.Status = "COMPLETE";

			//Notify user the success is complete.
			std::cout << "\nProject " << Project::PROJECT_NAME << " rendered successfully.\n\n";

			//Move the AVI file to the encoder
			LogFile::WriteToLog("Moving AVI file to AETL-Encoder hot folder...");
			TERMINATE_IF_FAILURE(AviCleanupUnsafe, &Project::PROJECT_NAME, &renderResult, LogFile::POST_RENDER);


		}
		//the process failed to read and didnt create the video.
		else
		{
			LogFile::WriteToLog("Render unsuccessful. Check the render output log for more information.");
			Project::FINAL_RENDER_FILEPATH = Dir::HotFolder + "\\" + FAIL_DIRECTORY + "\\" + Project::SQL_DATA.LocationID + "\\" + Project::TIMESTAMP;
			Project::SQL_DATA.Directory = "NULL";

			if (Project::RENDER_DATA.Retries >= 3)
			{
				Project::RENDER_DATA.Status = "FAILED";
				cout << "Rendering attempts is at limit. Moving project to FAILED directory." << endl;
				LogFile::WriteToLog("Rendering attempts at max. Moving project to FAILED directory.");
			}
			else
			{
				Project::RENDER_DATA.Status = "RETRY";
				Project::RENDER_DATA.Retries++;
			}

			//Notify user the failure is complete.
			std::cout << "\nProject " << Project::PROJECT_NAME << " failed to render properly.\n\n";

			//Delete the AVI file if there is anything to cleanup
			LogFile::WriteToLog("Deleting AVI file...");
			TERMINATE_IF_FAILURE(AviCleanupUnsafe, &Project::PROJECT_NAME, &renderResult, LogFile::POST_RENDER);
		}

		Project::RENDER_DATA.Directory = Project::SQL_DATA.Directory;
		//Add host name to the output log
		LogFile::WriteToLog("Adding host name to output log.");
		AppendHostName(out);

		//Store all the data in either archive or fail based on log info
		if (Project::RENDER_DATA.Status != "RETRY")
		{
			//Make the post-render folder
			LogFile::WriteToLog("Creating post-render folder " + Project::FINAL_RENDER_FILEPATH);
			TERMINATE_IF_FAILURE(CreateDirectoryUnsafe, &Project::FINAL_RENDER_FILEPATH, nullptr, LogFile::POST_RENDER);

			//Move the file over
			LogFile::WriteToLog("Moving project file to post-render folder.");
			fs::path returnPath = fs::path(Project::FINAL_RENDER_FILEPATH + "\\" + Project::TIMESTAMPED_FILENAME);
			TERMINATE_IF_FAILURE(RenameFileUnsafe, &Project::CURRENT_PROJECT_PATH, &returnPath, LogFile::POST_RENDER);

			//Create the output log for review.
			LogFile::WriteToLog("Creating output log.");
			string outputLogDirectory = Project::FINAL_RENDER_FILEPATH + "\\" + Project::TIMESTAMPED_FILENAME;
			TERMINATE_IF_FAILURE(CreateOutputLogUnsafe, &outputLogDirectory, &out, LogFile::PROJECT_FILED_ARCHIVED);
			LogFile::WriteToLog("Output log created.");

			//clean up adobes mess
			LogFile::WriteToLog("Moving Adobe logs to post-render folder (ONLY FOR VERSIONS 2019 OR LATER).");
			for (unsigned int i = 0; i < aeLogs.size(); i++)
			{
				fs::path oldFile = Project::CURRENT_RENDERING_DIRECTORY + "\\" + Project::TIMESTAMPED_FILENAME + " Logs" + "\\" + aeLogs[i];
				fs::path newFile = Project::FINAL_RENDER_FILEPATH + "\\" + aeLogs[i];
				LogFile::WriteToLog("Renaming " + oldFile.string() + " to " + newFile.string());
				TERMINATE_IF_FAILURE(RenameFileUnsafe, &oldFile, &newFile, LogFile::PROJECT_FILED_ARCHIVED);
			}

			//add our log to the render list and adjust the active render log we input earlier to be finished
			LogFile::WriteToLog("Adding SQLite Database Completed Render log.");
			TERMINATE_IF_FAILURE(AddRenderLogUnsafe, &Project::FINAL_RENDER_FILEPATH, nullptr, LogFile::PROJECT_FILED_ARCHIVED);
		}
		//if this is a retry then move the project file back into the project hot folder
		else
		{
			LogFile::WriteToLog("Moving project to hot folder for retry.");
			fs::path backToHotFolder = fs::path(Dir::HotFolder + "\\" + Project::PROJECT_NAME + ".aep");
			TERMINATE_IF_FAILURE(RenameFileUnsafe, &Project::CURRENT_PROJECT_PATH, &backToHotFolder, LogFile::POST_RENDER);
		}

		//adjust our active rendering log
		LogFile::WriteToLog("Adjusting SQLite Database Active Rendering log.");
		TERMINATE_IF_FAILURE(AdjustActiveRenderingDataUnsafe, nullptr, nullptr, LogFile::PROJECT_FILED_ARCHIVED);

		//cleanup the working directory
		LogFile::WriteToLog("Removing rendering directory and anything within.");
		TERMINATE_IF_FAILURE(RemoveDirectoryUnsafe, &Project::CURRENT_RENDERING_DIRECTORY, nullptr, LogFile::FULL_RENDER);
		LogFile::WriteToLog("---------- END OF RENDER SEQUENCE ----------");


		//wipe the project data since we successfully rendered.
		Project::Reset();

		LoopExecutionComplete = true;
	}
}

void CleanupRenderMess()
{
	LogFile::WriteToLog(std::string("Running cleanup with error code ") + std::to_string((int)LastThreadRenderProgress) + "...");
	//we need to check at which point the render failed to decipher this
	switch (LastThreadRenderProgress)
	{
		//nothing really needs to be done here since it didn't actually attach a project
	case LogFile::PRE_RENDER:	
		return;

		//if it failed here: the project was moved to the rendering directory but the render didnt actually start yet. We need to move the file back to the hot folder.
	case LogFile::ATTACHED_PROJECT:		
	{
		LogFile::WriteToLog("Cleanup: returning failed project back to hot folder.");
		fs::path backToHotFolder = fs::path(Dir::HotFolder + "\\" + Project::PROJECT_NAME + ".aep");

		if (EnsureSafeExecution(RenameFileUnsafe, &Project::CURRENT_PROJECT_PATH, &backToHotFolder) == false)
		{
			LogFile::WriteToLog("Cleanup failed. The project file could not be salvaged.");
		}

		//Adjust the Active Rendering Data table for this project.
		{
			Project::RENDER_DATA.Status = "RETRY";
			if (EnsureSafeExecution(AdjustActiveRenderingDataUnsafe, nullptr, nullptr))
			{
				LogFile::WriteToLog("Cleanup failed. The database ActiveRenderingData could not be updated.");
			}
		}

		return;
	}
		//if it failed here: the render process itself failed. We need to move the file back to the hot folder.
	case LogFile::DURING_RENDER:
	{
		LogFile::WriteToLog("Cleanup: returning failed project back to hot folder.");
		fs::path backToHotFolder = fs::path(Dir::HotFolder + "\\" + Project::PROJECT_NAME + ".aep");

		if (EnsureSafeExecution(RenameFileUnsafe, &Project::CURRENT_PROJECT_PATH, &backToHotFolder) == false)
		{
			LogFile::WriteToLog("Cleanup failed. The project file could not be salvaged.");
		}

		//Adjust the Active Rendering Data table for this project.
		{
			Project::RENDER_DATA.Status = "RETRY";
			if (EnsureSafeExecution(AdjustActiveRenderingDataUnsafe, nullptr, nullptr))
			{
				LogFile::WriteToLog("Cleanup failed. The database ActiveRenderingData could not be updated.");
			}
		}

		return;
	}
		//if it failed here: the render completed but the avi failed to be transfered.  We need to attempt to move the file back to the hot folder and delete the old AVI file.
	case LogFile::POST_RENDER:
	{
		LogFile::WriteToLog("Cleanup: deleting corrupted AVI file.");
		bool renderResult = false;
		if (EnsureSafeExecution(AviCleanupUnsafe, &Project::PROJECT_NAME, &renderResult) == false)
		{
			LogFile::WriteToLog("Cleanup failed. The AVI file could not be deleted.");
		}

		LogFile::WriteToLog("Cleanup: returning failed project back to hot folder.");
		fs::path backToHotFolder = fs::path(Dir::HotFolder + "\\" + Project::PROJECT_NAME + ".aep");

		if (EnsureSafeExecution(RenameFileUnsafe, &Project::CURRENT_PROJECT_PATH, &backToHotFolder) == false)
		{
			LogFile::WriteToLog("Cleanup failed. The project file could not be salvaged.");	
		}

		//Adjust the Active Rendering Data table for this project.
		{
			Project::RENDER_DATA.Status = "RETRY";
			if (EnsureSafeExecution(AdjustActiveRenderingDataUnsafe, nullptr, nullptr))
			{
				LogFile::WriteToLog("Cleanup failed. The database ActiveRenderingData could not be updated.");
			}
		}
	}
		return;

		//if it failed here: the project avi moved successfully but wasn't archived. We need to update the SQLite database. the archiving of the files really matter
	case LogFile::PROJECT_AVI_MIGRATED:

		//if it failed here: the project files were archived. We still need to attempt to update the SQLite database.
	case LogFile::PROJECT_FILED_ARCHIVED:
		LogFile::WriteToLog("Cleanup: updating SQLite database.");
		bool sqliteCallbackOne, sqliteCallbackTwo;

		sqliteCallbackOne = EnsureSafeExecution(AddRenderLogUnsafe, &Project::FINAL_RENDER_FILEPATH, nullptr);
		sqliteCallbackTwo = EnsureSafeExecution(AdjustActiveRenderingDataUnsafe, nullptr, nullptr);

		if (sqliteCallbackOne == false && sqliteCallbackTwo == false)
		{
			LogFile::WriteToLog("Cleanup failed. The SQLite database file could not be updated.");
		}

		//if it failed here: everything else succeeded other than cleaning up an old directory which doesnt really matter so its fine
	case LogFile::FULL_RENDER:
	default:
		return;
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
			ProgramExecutionComplete = true;
			break;
		}
	}
}


