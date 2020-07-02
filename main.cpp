#include "ListenerPCH.h"
#include <windows.h>
#include <conio.h>
#include <fstream>
#include <WinInet.h>


#define RENDER(_PROJECT) "-project \"" + _PROJECT + "\""

#define TERMINATE_IF_FAILURE(_FUNC_, _VALONE_, _VALTWO_, _ERR_) if (EnsureSafeExecution(_FUNC_, _VALONE_, _VALTWO_) == false) { LoopExecutionComplete = true; LastThreadRenderProgress = _ERR_;LastThreadExecution = LogFile::ERROR_STUCK_IN_ERROR; return;}

#define AETL_VERSION "0.91c - FFMPEG branch -AVI fix"

using namespace std;
namespace fs = std::filesystem;
using namespace UNSAFE;

//Our update loop
void VideoRenderUpdateLoop();

//Check if we're allowed to work
bool CheckToSeeIfHotFolderIsLocked();

//Get our FFMPEG location
std::string GetFFMPEGAbsoluteLocation();

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

	//load our hot folder from arguments
	for (int i = 0; i < argc; i++)
	{
		if (string(argv[i]) == "-i")
			if (i + 1 < argc)
				Settings::HotFolder = string(argv[i + 1]);
			else
			{
				std::cout << "-i flag found but no argument." << endl;
				ProgramExecutionComplete = true;
			}
		if (string(argv[i]) == "-ae")
			if (i + 1 < argc)
			{
				Settings::RenderPath = string(argv[i + 1]);

				//get our version. If this find fails then the path is messed up anyway so lets get out.
				string adobeVersion = Settings::RenderPath.substr(Settings::RenderPath.find("Adobe After Effects CC "), 27);

				if (adobeVersion == "")
				{
					std::cout << "Adobe path given is incorrect. Expected C:\\Program Files\\Adobe\\Adobe After Effects CC XXXX\\Support Files\\aerender.exe where XXXX is the version." << endl
						<< "Please locate your installation and rerun program with a correct path to it." << endl;
					ProgramExecutionComplete = true;
				}

				Settings::ADOBE_VERSION = atoi(adobeVersion.substr(adobeVersion.size() - 4).c_str());
			}
			else
			{
				std::cout << "-ae flag found but no argument." << endl;
				ProgramExecutionComplete = true;
			}
		if (string(argv[i]) == "-db")
			if (i + 1 < argc)
				Settings::DatabasePath = string(argv[i + 1]);
			else
			{
				std::cout << "-db flag found but no argument." << endl;
				ProgramExecutionComplete = true;
			}

		if (string(argv[i]) == "-o")
			if (i + 1 < argc)
				Settings::OutputFolder = string(argv[i + 1]);
			else
			{
				std::cout << "-o flag found but no argument." << endl;
				ProgramExecutionComplete = true;
			}

		if (string(argv[i]) == "-e")
			if (i + 1 < argc)
				Settings::EncodeFolder = string(argv[i + 1]);
			else
			{
				std::cout << "-e flag found but no argument." << endl;
				ProgramExecutionComplete = true;
			}

		if (string(argv[i]) == "-usage")
			if (i + 1 < argc)
				Settings::PercentThreshold = stod(argv[i + 1]);
			else
			{
				std::cout << "-usage flag found but no argument." << endl;
				ProgramExecutionComplete = true;
			}

		if (string(argv[i]) == "-sqlite")
		{
			std::cout << "Using SQLite3." << endl;
			Settings::UsingSqlite = true;
		}

		if (string(argv[i]) == "-res")
		{
			if (i + 1 < argc)
			{
				std::string argList = argv[i + 1];

				//Get the delimited list
				while (argList.find(',') != std::string::npos)
				{
					Settings::ResolutionsToEncode.push_back(argList.substr(0, argList.find(',')));
					argList = argList.substr(argList.find(',') + 1);
				}

				//Add final one
				Settings::ResolutionsToEncode.push_back(argList);
			}
			else
			{
				std::cout << "-res flag found but no argument." << endl;
				ProgramExecutionComplete = true;
			}
		}

		if (string(argv[i]) == "-test")
		{
			Settings::IsTestMode = true;
			std::cout << "Running in test mode." << std::endl;
		}
	}

	//Catches to see if the input commands are not set
	{
		if (Settings::UsingSqlite && Settings::DatabasePath == "")
		{
			std::cout << "Sqlite3 requested but database not specified.\n";
			ProgramExecutionComplete = true;
		}
		if (Settings::HotFolder == "")
		{
			std::cout << "Hot folder not set properly.\n";
			ProgramExecutionComplete = true;
		}
		if (Settings::RenderPath == "")
		{
			std::cout << "Adobe Render path not set properly. \n";
			ProgramExecutionComplete = true;
		}
		if (Settings::OutputFolder == "")
		{
			std::cout << "Output folder path not set properly. \n";
			ProgramExecutionComplete = true;
		}
		if (Settings::ResolutionsToEncode.size() == 0)
		{
			std::string out = "Warning: no resolutions supplied in argument list. Will not encode any videos.";
			std::cout << out << endl;
			LogFile::WriteToLog(out);
		}
		if (Settings::EncodeFolder == "")
		{
			std::cout << "Encode folder path not set properly. \n";
			ProgramExecutionComplete = true;
		}
	}

	//Load whichever database we're using
	if (Settings::UsingSqlite)
	{
		if (SQL::SQL_LoadDatabase(&Project::OUR_DATABASE, Settings::DatabasePath) == false)
		{
			std::cout << "Database failed to open" << endl;
			LogFile::WriteToLog("Database failed to open");
			ProgramExecutionComplete = true;
		}
		else
			LogFile::WriteToLog("Database loaded successfully.");
	}
	else
	{
		try
		{
			PGSQL::Connect("postgresql://aetl@10.20.2.6:5432/aetl?password=BU1yCdsDAvobR7N9srv3", AETL_DB);
		}
		catch (const std::exception & e)
		{
			//error reason
			std::cerr << e.what() << std::endl;

			LogFile::WriteToLog("Error upon attempting connection: " + std::string(e.what()));
			ProgramExecutionComplete = true;
		}
	}

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
		+ "\n\t\t\tHot folder: " + Settings::HotFolder
		+ "\n\t\t\tOutput folder: " + Settings::OutputFolder
		+ "\n\t\t\tRenderer: " + Settings::RenderPath
		+ "\n\t\t\tAdobe Version: " + to_string(Settings::ADOBE_VERSION)
		+ "\n\t\t\tMax Percent threshold: " + to_string(Settings::PercentThreshold)
		+ "\n\t\t\tUsing SQLite3: " + (Settings::UsingSqlite ? "true" : "false")
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
			string outputDrive = Settings::OutputFolder.substr(0, Settings::OutputFolder.find("\\") + 1);
			EnsureSafeExecution(UNSAFE::FreeSpaceAvailable, &checkDisk, &outputDrive);

			//if the disk is above usage specified, don't update. wait and try again.
			if (checkDisk.PercentUsed > Settings::PercentThreshold)
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
		LoopExecutionComplete = true;
		return;
	}
	//otherwise we fetch a file. 
	else
	{
		//create a directory iterator
		fs::directory_iterator directoryIterator;
		TERMINATE_IF_FAILURE(GetDirectoryIterator, &Settings::HotFolder, &directoryIterator, LogFile::PRE_RENDER);

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
		TERMINATE_IF_FAILURE(FetchProjectBuildLogUnsafe, nullptr, nullptr, LogFile::PRE_RENDER);
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
		FindAndReplaceAll(Project::PROJECT_NAME, Settings::HotFolder + "\\", "");

		//Create a stamped filename
		Project::TIMESTAMP = CurrentDateTime();
		LogFile::WriteToLog("Timestamp used for this render: " + Project::TIMESTAMP);
		Project::TIMESTAMPED_FILENAME = Project::PROJECT_NAME + "_" + Project::TIMESTAMP + ".aep";
		LogFile::WriteToLog("Locked filename: " + Project::TIMESTAMPED_FILENAME);

		//Create sub folders for this project if they arent already there
		LogFile::WriteToLog("Creating project sub folders.");
		{
			string dir = Settings::HotFolder + "\\" + ARCHIVE_DIRECTORY + "\\" + Project::SQL_DATA.LocationID;
			TERMINATE_IF_FAILURE(CreateDirectoryUnsafe, &dir, nullptr, LogFile::PRE_RENDER);
			dir = Settings::HotFolder + "\\" + ACTIVE_RENDER_DIECTORY + "\\" + Project::SQL_DATA.LocationID;
			TERMINATE_IF_FAILURE(CreateDirectoryUnsafe, &dir, nullptr, LogFile::PRE_RENDER);
			dir = Settings::HotFolder + "\\" + FAIL_DIRECTORY + "\\" + Project::SQL_DATA.LocationID;
			TERMINATE_IF_FAILURE(CreateDirectoryUnsafe, &dir, nullptr, LogFile::PRE_RENDER);
		}

		//create our new project directory and rename the file
		{
			string currentRenderingDirectory = Settings::HotFolder + "\\" + ACTIVE_RENDER_DIECTORY + "\\" + Project::SQL_DATA.LocationID + "\\" + Project::TIMESTAMP;
			LogFile::WriteToLog("Creating directory : " + currentRenderingDirectory);
			TERMINATE_IF_FAILURE(CreateDirectoryUnsafe, &currentRenderingDirectory, nullptr, LogFile::PRE_RENDER);
			Project::CURRENT_RENDERING_DIRECTORY = currentRenderingDirectory;
		}

		//Attempt to move the project to a controlled active rendering folder that is unique. Update the CURRENT_PROJECT_PATH if successful.
		fs::path attemptToMoveProjectPath = fs::absolute(Settings::HotFolder + "\\" + ACTIVE_RENDER_DIECTORY + "\\" + Project::SQL_DATA.LocationID + "\\" + Project::TIMESTAMP + "\\" + Project::TIMESTAMPED_FILENAME);
		LogFile::WriteToLog("Moving project file to " + attemptToMoveProjectPath.string());
		TERMINATE_IF_FAILURE(RenameFileUnsafe, &projectPath, &attemptToMoveProjectPath, LogFile::PRE_RENDER);
		Project::CURRENT_PROJECT_PATH = attemptToMoveProjectPath;

		//Add a log to the active rendering table notifying that we're starting a render
		Project::SQL_DATA.Directory = Settings::OutputFolder + "\\" + Project::PROJECT_NAME + ".avi";
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

				//Notify the database we are attempting a retry
				TERMINATE_IF_FAILURE(AdjustActiveRenderingDataUnsafe, nullptr, nullptr, LogFile::ATTACHED_PROJECT);
			}
			else
				//it's a fresh project. Build the RENDER_DATA using SQL_DATA and create a new entry in the database.
			{
				std::cout << "Project " << Project::PROJECT_NAME << " is a new project. Attempting first render." << endl;
				LogFile::WriteToLog("Project " + Project::PROJECT_NAME + ": adjusting render log.");
				Project::RENDER_DATA.ImageType = Project::SQL_DATA.ImageType;
				Project::RENDER_DATA.LocationID = Project::SQL_DATA.LocationID;
				Project::RENDER_DATA.ProjectID = Project::SQL_DATA.ProjectID;
				Project::RENDER_DATA.ProjectType = Project::SQL_DATA.ProjectType;
				Project::RENDER_DATA.Retries = 0;
				Project::RENDER_DATA.Directory = Project::SQL_DATA.Directory;
				Project::RENDER_DATA.Status = "RENDERING";

				//Add/adjust entry to the database
				TERMINATE_IF_FAILURE(AddActiveRenderingDataUnsafe, nullptr, nullptr, LogFile::ATTACHED_PROJECT);
			}
		}

		//notify the render process
		std::cout << "Project " << Project::PROJECT_NAME << " now being rendered. Locking....\n\n";
		LogFile::WriteToLog("Project " + Project::PROJECT_NAME + " is now being rendered.");

		//render the video
		std::string command = Settings::RenderPath + " " + RENDER(Project::CURRENT_PROJECT_PATH.string());
		std::string out;
		LogFile::WriteToLog("Executing command: " + command);
		TERMINATE_IF_FAILURE(AttemptVideoRender, &command, &out, LogFile::DURING_RENDER);

		//sleep for a few seconds after exiting to ensure all files are written
		SLEEP(3000);
		LogFile::WriteToLog("Rendering completed.");

		//copy log names to vector to move to a different location
		vector<string> aeLogs;
		if (Settings::ADOBE_VERSION > 2018)
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
			Project::FINAL_RENDER_FILEPATH = Settings::HotFolder + "\\" + ARCHIVE_DIRECTORY + "\\" + Project::SQL_DATA.LocationID + "\\" + Project::TIMESTAMP;
			Project::SQL_DATA.Directory = Settings::OutputFolder + "\\" + Project::PROJECT_NAME + ".avi";
			Project::RENDER_DATA.Status = "COMPLETE";

			//Notify user the success is complete.
			std::cout << "\nProject " << Project::PROJECT_NAME << " rendered successfully.\n\n";

			//Move the AVI file to the encoder
			//LogFile::WriteToLog("Moving AVI file to AETL-Encoder hot folder...");
			//TERMINATE_IF_FAILURE(AviCleanupUnsafe, &Project::PROJECT_NAME, &renderResult, LogFile::POST_RENDER);


		}
		//the process failed to read and didnt create the video.
		else
		{
			LogFile::WriteToLog("Render unsuccessful. Check the render output log for more information.");
			Project::FINAL_RENDER_FILEPATH = Settings::HotFolder + "\\" + FAIL_DIRECTORY + "\\" + Project::SQL_DATA.LocationID + "\\" + Project::TIMESTAMP;
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


		//If the status is retry then we failed rendering so we can't encode any AVI file. Time to cleanup and leave.
		if (Project::RENDER_DATA.Status == "RETRY")
		{
			LogFile::WriteToLog("Moving project to hot folder for retry.");
			fs::path backToHotFolder = fs::path(Settings::HotFolder + "\\" + Project::PROJECT_NAME + ".aep");
			TERMINATE_IF_FAILURE(RenameFileUnsafe, &Project::CURRENT_PROJECT_PATH, &backToHotFolder, LogFile::POST_RENDER);

			//adjust our active rendering log
			LogFile::WriteToLog("Adjusting SQLite Database Active Rendering log.");
			TERMINATE_IF_FAILURE(AdjustActiveRenderingDataUnsafe, nullptr, nullptr, LogFile::PROJECT_FILED_ARCHIVED);

			//cleanup the working directory
			LogFile::WriteToLog("Removing rendering directory and anything within.");
			TERMINATE_IF_FAILURE(RemoveDirectoryUnsafe, &Project::CURRENT_RENDERING_DIRECTORY, nullptr, LogFile::FULL_RENDER);
			LogFile::WriteToLog("---------- END OF AE RENDER SEQUENCE ----------");


			//wipe the project data since we successfully completed a loop.
			Project::Reset();

			LoopExecutionComplete = true;
			return;
		}

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
		std::string finalOutputFileName = outputLogDirectory + FAIL_FILE;
		TERMINATE_IF_FAILURE(CreateOutputLogUnsafe, &finalOutputFileName, &out, LogFile::PROJECT_FILED_ARCHIVED);
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

		//adjust our active rendering log
		LogFile::WriteToLog("Adjusting SQLite Database Active Rendering log.");
		TERMINATE_IF_FAILURE(AdjustActiveRenderingDataUnsafe, nullptr, nullptr, LogFile::PROJECT_FILED_ARCHIVED);

		//cleanup the working directory
		LogFile::WriteToLog("Removing rendering directory and anything within.");
		TERMINATE_IF_FAILURE(RemoveDirectoryUnsafe, &Project::CURRENT_RENDERING_DIRECTORY, nullptr, LogFile::FULL_RENDER);
		LogFile::WriteToLog("---------- END OF AE RENDER SEQUENCE ----------");


		//Now we start the encoding sequence
		LogFile::WriteToLog("---------- START OF FFMPEG ENCODING SEQUENCE ----------");

		std::string ffmpegExeLocation = GetFFMPEGAbsoluteLocation();
		std::string aviFileLocation = Settings::OutputFolder + "\\" + Project::PROJECT_NAME + ".avi";

		std::vector<std::string> generatedVideoFiles;
		generatedVideoFiles.reserve(Settings::ResolutionsToEncode.size());

		//Perform the encoding process for each resolution specified in the argument list
		for (int i = 0; i < Settings::ResolutionsToEncode.size(); i++)
		{
			std::string outputSubdirectory = Settings::ResolutionsToEncode[i];
			std::string vfCommand = "-vf ";

			//Is this string a number? Format the vf accordingly
			if (!outputSubdirectory.empty() && std::all_of(outputSubdirectory.begin(), outputSubdirectory.end(), ::isdigit))
			{
				outputSubdirectory += "p";
				vfCommand += "scale=-2:" + Settings::ResolutionsToEncode[i];
			}
			else
			{
				//it's a non-number so we're just going to use native resolution since thats PROBABLY whats intended
				vfCommand += "\"pad=ceil(iw/2)*2:ceil(ih/2)*2\""; //On the off-chance our image isn't divisible by 2 (what h264 needs) we'll round it and add pad space if division makes a hole.
			}

			std::string outputFile = Settings::EncodeFolder + "\\" + outputSubdirectory + "\\" + Project::PROJECT_NAME + ".mp4";
			generatedVideoFiles.push_back(outputFile);

			//EXAMPLE FORMAT
			std::string command = "\"" + ffmpegExeLocation + "\" -i \"" + aviFileLocation + "\" -preset veryfast -pix_fmt yuv420p " + vfCommand + " -f mp4 \"" + outputFile + "\"";

			//encode the video
			std::string ffmpegSTDOUT;
			LogFile::WriteToLog("Executing command: " + command);
			TERMINATE_IF_FAILURE(AttemptVideoEncode, &command, &ffmpegSTDOUT, LogFile::DURING_ENCODE);

			//If the output has any error text then it's probably fucked.
			bool success = ffmpegSTDOUT.find("Error") == std::string::npos;

			//Display outcome to user
			std::string encodeResponse = success ? "Encoded video " + outputFile + ":" + Settings::ResolutionsToEncode[i] + " successful." : "Encoded video " + outputFile + ":" + Settings::ResolutionsToEncode[i] + " failed.";
			std::cout << encodeResponse << std::endl;
			LogFile::WriteToLog(encodeResponse);

			//Handle cleaning up the file
			TERMINATE_IF_FAILURE(EncodeCleanup, &outputFile, &success, LogFile::DURING_ENCODE);

			//Add the output log to the folder
			{
				LogFile::WriteToLog("Adding host name to output log.");
				AppendHostName(ffmpegSTDOUT);

				std::string finalOutputFileName = outputLogDirectory + Settings::ResolutionsToEncode[i] + FAIL_FILE;
				TERMINATE_IF_FAILURE(CreateOutputLogUnsafe, &finalOutputFileName, &ffmpegSTDOUT, LogFile::DURING_ENCODE);
			}

			//It should never fail encoding a video if the AVI generated by aerender is clean. So if this actually happens somehow we need to completely scrap this project.
			if (success == false)
			{
				//Set us as failed now and re-update the database with this newly discovered failure
				Project::RENDER_DATA.Status = "FAILED";
				TERMINATE_IF_FAILURE(AdjustActiveRenderingDataUnsafe, nullptr, nullptr, LogFile::DURING_ENCODE);

				//Delete the source AVI file.
				LogFile::WriteToLog("Deleting AVI file...");
				TERMINATE_IF_FAILURE(AviCleanupUnsafe, &Project::PROJECT_NAME, &success, LogFile::DURING_ENCODE);

				//Try to remove any mp4s that might be persisting.
				LogFile::WriteToLog("Deleting all potentially encoded project video files...");
				TERMINATE_IF_FAILURE(DeleteAllEncodedVideosForProject, nullptr, nullptr, LogFile::DURING_ENCODE);

				Project::Reset();
				LoopExecutionComplete = true;
				return;
			}
		}

		//Delete the AVI file now that its served its purpose
		{
			LogFile::WriteToLog("Deleting AVI file...");
			bool success = false;
			EnsureSafeExecution(AviCleanupUnsafe, &Project::PROJECT_NAME, &success);
		}

		//Now we end the encoding sequence
		LogFile::WriteToLog("---------- END OF FFMPEG ENCODING SEQUENCE ----------");

		//Now we start the upload sequence
		LogFile::WriteToLog("---------- START OF UPLOAD SEQUENCE ----------");

		auto placeProjectFileBackToHotFolder = []() 
		{
			//Move the project back into the hot folder for retry.
			fs::path backToHotFolder = fs::path(Settings::HotFolder + "\\" + Project::PROJECT_NAME + ".aep");
			fs::path returnPath = fs::path(Project::FINAL_RENDER_FILEPATH + "\\" + Project::TIMESTAMPED_FILENAME);
			TERMINATE_IF_FAILURE(RenameFileUnsafe, &returnPath, &backToHotFolder, LogFile::POST_RENDER);

			Project::Reset();

			LoopExecutionComplete = true;
		};

		//if we made it this far it means we've finished encoding all our videos and now it's time to upload them to the endpoint 1 by 1
		for (int i = 0; i < generatedVideoFiles.size(); i++)
		{
			bool UploadIncomplete = true;
			int retryCount = 0;

			while (UploadIncomplete)
			{
				auto res = AETL_Upload::UploadUsingCurl(std::filesystem::path(generatedVideoFiles[i]), nullptr);

				switch (res)
				{
				case AETL_Upload::UploadResponseCodes::SUCCESS:
					UploadIncomplete = false;
					break;
				case AETL_Upload::UploadResponseCodes::FAILED_UPLOAD_FAILED://attempt a ping to google to make sure internet works
				{
					bool bConnect = false;

					while (bConnect == false)
					{
						char url[128];
						strcat(url, "http://www.google.com");
						bConnect = InternetCheckConnection(url, FLAG_ICC_FORCE_CONNECTION, 0);

						std::string res = bConnect ? "Internet ping to google successful." : "Internet ping to google unsuccessful. Attempting again in 1 minute.";
						std::cout << res << std::endl;
						LogFile::WriteToLog(res);

						//Only ping once a minute.
						if (bConnect == false)
							SLEEP(1000 * 60);
					}
				}
					break;
				case AETL_Upload::UploadResponseCodes::FAILED_FILE_NOT_DELETED://check if the file is still there and attempt to remove it if so
				{		
					bool exists = true;
					TERMINATE_IF_FAILURE(ObjectExistsUnsafe, &generatedVideoFiles[i], &exists, LogFile::DURING_ENCODE);
					
					if (exists)
						exists = remove(generatedVideoFiles[i].c_str());

					UploadIncomplete = exists;
				}
					break;
				case AETL_Upload::UploadResponseCodes::FAILED_CURL_NOT_INSTALLED:
					//if curl isn't even there we need to copy curl over and fix it.
					LogFile::WriteToLog("Attempting to copy curl from AETL...");
					bool successfulCopy = false;
					TERMINATE_IF_FAILURE(CopyCurlFolderToSource, nullptr, &successfulCopy, LogFile::DURING_ENCODE);

					//if we didnt copy it then something is really fucked so we're going to put the project back into the hot folder and clean everything up.
					if (successfulCopy == false)
					{
						std::cout << "Failed to copy over curl... aborting process." << std::endl;

						//Try to remove any mp4s that might be persisting.
						LogFile::WriteToLog("Deleting all potentially encoded project video files...");
						TERMINATE_IF_FAILURE(DeleteAllEncodedVideosForProject, nullptr, nullptr, LogFile::DURING_ENCODE);

						//We need to remove the video-rendered info we put on this project
						if (Settings::UsingSqlite == false)
						{
							(void)PGSQL::Query("UPDATE public.\"" + std::string(DATABASE_PROJECT_LOG) + "\" SET \"VideoRendered\"=NULL, \"UpdatedAt\"=CURRENT_TIMESTAMP"
								+ " WHERE \"Name\"='" + Project::PROJECT_NAME + "';", AETL_DB);
						}

						//remove the project from the archive and exit out
						placeProjectFileBackToHotFolder();
						ProgramExecutionComplete = true;
						return;
					}

					std::cout << "curl successfully copied. Re-attempting upload." << std::endl;
					LogFile::WriteToLog("curl successfully copied. Re-attempting upload.");
					break;
				}

				//don't go as fast as possible in case a loop happens for a while
				SLEEP(250);
			}
		}

		//if we got here then everything uploaded to where it needed to. We can successfully flag this project as uploaded and complete.
		if (Settings::UsingSqlite == false)
		{
			(void)PGSQL::Query("UPDATE public.\"" + std::string(DATABASE_PROJECT_LOG) + "\" SET \"Uploaded\"=CURRENT_TIMESTAMP, \"UpdatedAt\"=CURRENT_TIMESTAMP, \"Status\"=\"COMPLETE\""
				+ " WHERE \"Name\"='" + Project::PROJECT_NAME + "';", AETL_DB);
		}

		LogFile::WriteToLog("---------- END OF UPLOAD SEQUENCE ----------");



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
		fs::path backToHotFolder = fs::path(Settings::HotFolder + "\\" + Project::PROJECT_NAME + ".aep");

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
		fs::path backToHotFolder = fs::path(Settings::HotFolder + "\\" + Project::PROJECT_NAME + ".aep");

		if (EnsureSafeExecution(RenameFileUnsafe, &Project::CURRENT_PROJECT_PATH, &backToHotFolder) == false)
		{
			LogFile::WriteToLog("Cleanup failed. The project file could not be salvaged.");
		}

		//Adjust the Active Rendering Data table for this project.
		{
			Project::RENDER_DATA.Status = "RETRY";
			if (EnsureSafeExecution(AdjustActiveRenderingDataUnsafe, nullptr, nullptr) == false)
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
		fs::path backToHotFolder = fs::path(Settings::HotFolder + "\\" + Project::PROJECT_NAME + ".aep");

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

		//if it failed here we can go straight to encoding and try to salve this.
	case LogFile::FULL_RENDER:
	{
		//Now we start the encoding sequence
		LogFile::WriteToLog("---------- START OF FFMPEG ENCODING SEQUENCE ----------");

		string outputLogDirectory = Project::FINAL_RENDER_FILEPATH + "\\" + Project::TIMESTAMPED_FILENAME;
		std::string ffmpegExeLocation = GetFFMPEGAbsoluteLocation();
		std::string aviFileLocation = Settings::OutputFolder + "\\" + Project::PROJECT_NAME + ".avi";

		//Perform the encoding process for each resolution specified in the argument list
		for (int i = 0; i < Settings::ResolutionsToEncode.size(); i++)
		{
			std::string outputSubdirectory = Settings::ResolutionsToEncode[i];
			std::string vfCommand = "-vf ";

			//Is this string a number? Format the vf accordingly
			if (!outputSubdirectory.empty() && std::all_of(outputSubdirectory.begin(), outputSubdirectory.end(), ::isdigit))
			{
				outputSubdirectory += "p";
				vfCommand += "scale=-2:" + Settings::ResolutionsToEncode[i];
			}
			else
			{
				//it's a non-number so we're just going to use native resolution since thats PROBABLY whats intended
				vfCommand += "\"pad=ceil(iw/2)*2:ceil(ih/2)*2\""; //On the off-chance our image isn't divisible by 2 (what h264 needs) we'll round it and add pad space if division makes a hole.
			}

			std::string outputFile = Settings::EncodeFolder + "\\" + outputSubdirectory + "\\" + Project::PROJECT_NAME + ".lock";

			//EXAMPLE FORMAT
			std::string command = "\"" + ffmpegExeLocation + "\" -i \"" + aviFileLocation + "\" -preset veryfast -pix_fmt yuv420p " + vfCommand + " -f mp4 \"" + outputFile + "\"";

			//encode the video
			std::string ffmpegSTDOUT;
			LogFile::WriteToLog("Executing command: " + command);
			EnsureSafeExecution(AttemptVideoEncode, &command, &ffmpegSTDOUT);

			//If the output has any error text then it's probably fucked.
			bool succeded = ffmpegSTDOUT.find("Error") == std::string::npos;

			//Display outcome to user
			std::string encodeResponse = succeded ? "Encoded video " + outputFile + ":" + Settings::ResolutionsToEncode[i] + " successful." : "Encoded video " + outputFile + ":" + Settings::ResolutionsToEncode[i] + " failed.";
			std::cout << encodeResponse << std::endl;
			LogFile::WriteToLog(encodeResponse);

			//Handle cleaning up the file
			EnsureSafeExecution(EncodeCleanup, &outputFile, &succeded);

			//Add the output log to the folder
			{
				LogFile::WriteToLog("Adding host name to output log.");
				AppendHostName(ffmpegSTDOUT);

				std::string finalOutputFileName = outputLogDirectory + Settings::ResolutionsToEncode[i] + FAIL_FILE;
				EnsureSafeExecution(CreateOutputLogUnsafe, &finalOutputFileName, &ffmpegSTDOUT);
			}

			//It should never fail encoding a video if the AVI generated by aerender is clean. So if this actually happens somehow we need to completely scrap this project.
			if (succeded == false)
			{
				//Set us as failed now and re-update the database with this newly discovered failure
				Project::RENDER_DATA.Status = "FAILED";
				EnsureSafeExecution(AdjustActiveRenderingDataUnsafe, nullptr, nullptr);

				//Delete the AVI file.
				LogFile::WriteToLog("Deleting AVI file...");
				EnsureSafeExecution(AviCleanupUnsafe, &Project::PROJECT_NAME, &succeded);

				return;
			}
		}

		//Now we end the encoding sequence
		LogFile::WriteToLog("---------- END OF FFMPEG ENCODING SEQUENCE ----------");


		//Delete the AVI file now that its served its purpose
		LogFile::WriteToLog("Deleting AVI file...");
		bool success = false;
		EnsureSafeExecution(AviCleanupUnsafe, &Project::PROJECT_NAME, &success);
	}
		break;
	case LogFile::DURING_ENCODE:
	{
		//We need to cleanup any and all incomplete videos and AVI files as well as tell the database we failed

		//Set us as failed now and re-update the database with this newly discovered failure
		Project::RENDER_DATA.Status = "FAILED";
		EnsureSafeExecution(AdjustActiveRenderingDataUnsafe, nullptr, nullptr);

		//Try to remove any mp4s that might be persisting.
		EnsureSafeExecution(DeleteAllEncodedVideosForProject, nullptr, nullptr);

		//Delete the AVI file.
		LogFile::WriteToLog("Deleting AVI file...");
		bool success = false;
		EnsureSafeExecution(AviCleanupUnsafe, &Project::PROJECT_NAME, &success);
	}
	default:
		return;
	}

}

bool CheckToSeeIfHotFolderIsLocked()
{
	bool isLocked = fs::exists(Settings::HotFolder + "\\" + LISTENER_FILE_NAME);

	//check all files in the directory
	if (isLocked)
	{
		std::cout << "Hot Folder is currently locked...\n";
		LogFile::WriteToLog("Hot Folder is currently locked.");
	}

	return isLocked;
}

std::string GetFFMPEGAbsoluteLocation()
{
	std::vector<char> buf(1024, 0);
	std::vector<char>::size_type size = buf.size();

	bool havePath = false;
	bool shouldContinue = true;
	do
	{
		//Fill buffer with the character list of our exe path
		DWORD result = GetModuleFileNameA(nullptr, &buf[0], (DWORD)size);
		DWORD lastError = GetLastError();
		if (result == 0)
		{
			shouldContinue = false;
		}
		else if (result < size)
		{
			havePath = true;
			shouldContinue = false;
		}
		else if (
			result == size
			&& (lastError == ERROR_INSUFFICIENT_BUFFER || lastError == ERROR_SUCCESS)
			)
		{
			size *= 2;
			buf.resize(size);
		}
		else
		{
			shouldContinue = false;
		}
	} while (shouldContinue);

	//Location of exe
	std::string absLocation = &buf[0];
	
	//Folder exists in same location as exe so simply strip off the EXE name and add the directory
	std::string ffmpegLocation = absLocation.substr(0, absLocation.find("AETL-Listener.exe")) + "ffmpeg\\bin\\ffmpeg.exe";

	return ffmpegLocation;
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


