#include "LogFile.h"
#include <experimental/filesystem>
#include <iostream>
#include <fstream>

std::ofstream LogFile::LogFileStream;

LogFile::LogFile()
{

}

void LogFile::BeginLogging()
{
	//create our Log folder if it isnt there
	_DirectoryExists("Logs");

	//Create the log file and begin streaming data
	std::string dateFormated = _CurrentDateTime();
	_FindAndReplaceAll(dateFormated, ":", ".");
	std::string LogPath = _GetAbsoluteDirectory("Logs") + "\\" + dateFormated + ".log";
	_FindAndReplaceAll(LogPath, "\\", "/");

	LogFileStream = std::ofstream(LogPath, std::ios::out);
}

void LogFile::WriteToLog(std::string data)
{
	if (LogFileStream.is_open())
	{
		LogFileStream << _CurrentDateTime() << " - " << data << std::endl;
	}
}

void LogFile::EndLogging()
{
	if (LogFileStream.is_open())
	{
		LogFileStream.close();
	}
}

static bool _DirectoryExists(std::string FolderPath, bool CreateDirectoryIfDoesNotExist /* = true */)
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

static void _FindAndReplaceAll(std::string& data, std::string toSearch, std::string replaceStr)
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

//Get a datetime stamp
const static std::string _CurrentDateTime() 
{
	time_t     now = time(0);
	struct tm  tstruct;
	char       buf[80];
	tstruct = *localtime(&now);

	strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

	std::string data(buf);

	_FindAndReplaceAll(data, ":", ".");

	return data;
}

//get our directory
const static std::string _GetAbsoluteDirectory(std::string Directory)
{
	return std::experimental::filesystem::absolute(Directory).string();
}
