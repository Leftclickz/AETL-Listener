#include "Helpers.h"
#include <time.h>
#include <fstream>
#include <experimental/filesystem>

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

const std::string GetAbsoluteDirectory(std::string Directory)
{
	return std::experimental::filesystem::absolute(Directory).string();
}

void RenameMultipleFilesToNewDirectory(std::vector<std::string> fileNames, std::string OldDirectory, std::string NewDirectory)
{
	for (unsigned int i = 0; i < fileNames.size(); i++)
	{
		std::experimental::filesystem::path oldFile = OldDirectory + "\\" + fileNames[i];
		std::experimental::filesystem::path newFile = NewDirectory + "\\" + fileNames[i];

		std::experimental::filesystem::rename(oldFile, newFile);
	}
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

void CreateLog(std::string filepath, std::string& data)
{
	std::ofstream output(filepath + FAIL_FILE);

	if (output.is_open())
	{
		output << data << std::endl;
		output.close();
	}
}