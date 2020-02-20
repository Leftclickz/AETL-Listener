#include "LogFile.h"
#include <filesystem>
#include <iostream>
#include <fstream>

std::ofstream LogFile::LogFileStream;

std::string* LogFile::LastFiveEntries;

int LogFile::CurrentEntryIndex;

int LogFile::CurrentReturnCodeIndex;

int* LogFile::LastFiveReturnCodes;

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

	LastFiveEntries = new std::string[LOG_ENTRY_COUNT_LIMIT]{ "" };
	LogFileStream = std::ofstream(LogPath, std::ios::out);
	LastFiveReturnCodes = new int[LOG_ENTRY_COUNT_LIMIT] {0};
	CurrentEntryIndex = 0;
	CurrentReturnCodeIndex = 0;
}

void LogFile::WriteToLog(std::string data)
{
	if (LogFileStream.is_open())
	{
		//Copy the entry into temporary memory
		LastFiveEntries[CurrentEntryIndex] = data;

		CurrentEntryIndex++;
		CurrentEntryIndex = CurrentEntryIndex >= LOG_ENTRY_COUNT_LIMIT ? 0 : CurrentEntryIndex;

		//Log the data.
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



bool LogFile::IsInfinitelyWriting()
{
	if (LogFileStream.is_open())
	{
		std::string CurrentCompare = LastFiveEntries[0];

		for (int i = 1; i < LOG_ENTRY_COUNT_LIMIT; i++)
		{
			if (CurrentCompare == LastFiveEntries[i])
				continue;
			else
				return false;
		}
	}

	return false;
}

bool LogFile::IsStuckInError()
{
	for (int i = 0; i < LOG_ENTRY_COUNT_LIMIT; i++)
	{
		if (LastFiveReturnCodes[i] == -1)
			continue;
		else
			return false;
	}

	return true;
}

void LogFile::AddReturnCode(int val)
{
	LastFiveReturnCodes[CurrentReturnCodeIndex] = val;

	CurrentReturnCodeIndex++;
	CurrentReturnCodeIndex = CurrentReturnCodeIndex >= LOG_ENTRY_COUNT_LIMIT ? 0 : CurrentReturnCodeIndex;

}

static bool _DirectoryExists(std::string FolderPath, bool CreateDirectoryIfDoesNotExist /* = true */)
{
	namespace fs = std::filesystem;
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
	return std::filesystem::absolute(Directory).string();
}
