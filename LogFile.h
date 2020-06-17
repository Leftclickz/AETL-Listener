#pragma once
#include <string>

#define LOG_ENTRY_COUNT_LIMIT 5

class LogFile
{
public:
	LogFile();

	static void BeginLogging();

	static void WriteToLog(std::string data);

	static void EndLogging();

	static bool IsInfinitelyWriting();

	static bool IsStuckInError();

	static void AddReturnCode(int val);

	enum eThreadExecutionCodes
	{
		ERROR_STUCK_IN_ERROR = -2,
		ERROR_STUCK_IN_WRITING = -1,
		SUCCESSFUL = 0,
	};

	enum eProgressOfRender
	{
		PRE_RENDER = 0,
		ATTACHED_PROJECT = 1,
		DURING_RENDER = 2,
		POST_RENDER = 3,
		PROJECT_AVI_MIGRATED = 4,
		PROJECT_FILED_ARCHIVED = 5,
		FULL_RENDER = 6,
		DURING_ENCODE = 7
	};

private:

	static std::ofstream LogFileStream;
	static std::string* LastFiveEntries;
	static int CurrentEntryIndex;
	static int CurrentReturnCodeIndex;

	static int* LastFiveReturnCodes;

};

static bool _DirectoryExists(std::string FolderPath, bool CreateDirectoryIfDoesNotExist = true);

static void _FindAndReplaceAll(std::string& data, std::string toSearch, std::string replaceStr);

const static std::string _CurrentDateTime();

const static std::string _GetAbsoluteDirectory(std::string Directory);