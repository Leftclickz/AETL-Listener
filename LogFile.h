#pragma once
#include <string>

class LogFile
{
public:
	LogFile();

	static void BeginLogging();

	static void WriteToLog(std::string data);

	static void EndLogging();

private:

	static std::ofstream LogFileStream;
};

static bool _DirectoryExists(std::string FolderPath, bool CreateDirectoryIfDoesNotExist = true);

static void _FindAndReplaceAll(std::string& data, std::string toSearch, std::string replaceStr);

const static std::string _CurrentDateTime();

const static std::string _GetAbsoluteDirectory(std::string Directory);