#pragma once
#include <string>
#include <vector>



#define FAIL_FILE ".log"

//Replace an occurence with something else
void FindAndReplaceAll(std::string& data, std::string toSearch, std::string replaceStr);

//Get a datetime stamp
const std::string CurrentDateTime();

//get our directory
const std::string GetAbsoluteDirectory(std::string Directory);

//Import a bunch of files
void RenameMultipleFilesToNewDirectory(std::vector<std::string> fileNames, std::string OldDirectory, std::string NewDirectory);

//Check if a directory exists. Can create it if it doesn't exist.
bool DirectoryExists(std::string FolderPath, bool CreateDirectoryIfDoesNotExist = true);

//create a log with the data of what happened on an unsuccessful render
void CreateLog(std::string filepath, std::string& data);