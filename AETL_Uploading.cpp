#include "ListenerPCH.h"

namespace AETL_Upload
{

	UploadResponseCodes UploadUsingCurl(std::filesystem::path Filepath, std::string AuthKey)
	{
		std::string fullFilepath = GetAbsoluteDirectory(Filepath.string());

		if (std::filesystem::exists(CURL_EXE) == false)
		{
			std::cout << "CURL is missing from its installation path. Something went wrong." << std::endl;
			LogFile::WriteToLog("CURL is missing from its installation path. Something went wrong.");
			return AETL_Upload::UploadResponseCodes::FAILED_CURL_NOT_INSTALLED;
		}

		//Our parameters for upload
		UploadParams params;
		params.File += GetAbsoluteDirectory(Filepath.string());

		//get resolution from folder name
		std::string resolution = Filepath.parent_path().string();
		FindAndReplaceAll(resolution, Settings::HotFolder + "\\", "");
		params.Res += resolution;

		//get our filename and a copy stripped of the project ID
		Filepath.replace_extension("");
		std::string fileName = Filepath.filename().string();

		//Replace the location ID of the url with the project location id
		FindAndReplaceAll(params.URL, DEFAULT_LOCATION_ID, Project::RENDER_DATA.LocationID);

		//Get params based on filename being monthly or not
		if (fileName.find("MONTHLY") == string::npos)
			//FULL files have the structure {PROJECT_ID}-{LOCATION_ID}.mp4
		{
			params.Type += "FULL";
			std::string date = CurrentDateTime();//since it's a full timelapse we'll just send it to the current month it is and year
			params.Year += date.substr(0, 4);
			params.Month += date.substr(5, 2);
		}
		else
			//MONTHLY files have the structure {PROJECT_ID}-{LOCATION_ID}-MONTHLY-{YEAR}-{MONTH}.mp4
		{
			params.Type += "MONTHLY";
			params.Year += fileName.substr(fileName.find("MONTHLY-") + 8, 4);
			params.Month += fileName.substr(fileName.find("MONTHLY-") + 13, 2);
		}

		//the command we'll use
		std::string command = Settings::IsTestMode ? DEFAULT_TEST_COMMAND : DEFAULT_CURL_COMMAND;

		//our absolute path to curl
		std::string absCurlPath = "\"" + GetAbsoluteDirectory(CURL_EXE) + "\"";

		//Replace the default vals with the one of our file to upload
		FindAndReplaceAll(command, DEFAULT_CURL_PATH, absCurlPath);
		FindAndReplaceAll(command, DEFAULT_RES, params.Res);
		FindAndReplaceAll(command, DEFAULT_FILENAME, params.File);
		FindAndReplaceAll(command, DEFAULT_MONTH, params.Month);
		FindAndReplaceAll(command, DEFAULT_YEAR, params.Year);
		FindAndReplaceAll(command, DEFAULT_TYPE, params.Type);

		//We only change the URL and auth key if we're in production mode otherwise use the test vals
		if (Settings::IsTestMode == false)
		{
			FindAndReplaceAll(command, DEFAULT_URL, params.URL);
			FindAndReplaceAll(command, DEFAULT_TEST_AUTH, AuthKey);
		}

		std::cout << "Uploading file - " << Filepath << std::endl;
		LogFile::WriteToLog("Uploading file - " + Filepath.string());

		//------------- EXECUTION BEGIN ---------------

		//convert to wide format and execute the command
		wstring executeString(command.begin(), command.end());
		LogFile::WriteToLog("Executing command : " + absCurlPath);
		CStringA res = ExecCmd(executeString.c_str());

		//Check if return code was a success or not
		if (res.Find(CLEAN_RESPONSE_CODE) == ERROR_BAD_HTTP_RESPONSE)
		{
			std::cout << "Upload failed. Full response: " + res << std::endl;
			LogFile::WriteToLog(std::string("Upload failed. Full response: ") + res.GetString() + ".");
			return AETL_Upload::UploadResponseCodes::FAILED_UPLOAD_FAILED;
		}
		else
		{
			std::cout << "Upload successful." << std::endl;
			LogFile::WriteToLog(std::string("Upload successful. Full response: ") + res.GetString() + ".");

			//remove the file since we successfully uploaded it
			if (remove(fullFilepath.c_str()) == 0)
			{
				std::cout << fullFilepath + ": successfully deleted from system." << std::endl;
				LogFile::WriteToLog(fullFilepath + ": successfully deleted from system.");
			}
			else
			{
				std::cout << fullFilepath + ": failed to delete from system." << std::endl;
				LogFile::WriteToLog(fullFilepath + ": failed to delete from system.");
				return AETL_Upload::UploadResponseCodes::FAILED_FILE_NOT_DELETED;
			}
		}
		//------------- EXECUTION END --------------------

		return AETL_Upload::UploadResponseCodes::SUCCESS;
	}

	CStringA ExecCmd(const wchar_t* cmd)
	{
		CStringA strResult;
		HANDLE hPipeRead, hPipeWrite;

		SECURITY_ATTRIBUTES saAttr = { sizeof(SECURITY_ATTRIBUTES) };
		saAttr.bInheritHandle = TRUE; // Pipe handles are inherited by child process.
		saAttr.lpSecurityDescriptor = NULL;

		// Create a pipe to get results from child's stdout.
		if (!CreatePipe(&hPipeRead, &hPipeWrite, &saAttr, 0))
			return strResult;

		STARTUPINFOW si = { sizeof(STARTUPINFOW) };
		si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
		si.hStdOutput = hPipeWrite;
		si.hStdError = hPipeWrite;
		si.wShowWindow = SW_HIDE; // Prevents cmd window from flashing.
								  // Requires STARTF_USESHOWWINDOW in dwFlags.

		PROCESS_INFORMATION pi = { 0 };

		BOOL fSuccess = CreateProcessW(NULL, (LPWSTR)cmd, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
		if (!fSuccess)
		{
			CloseHandle(hPipeWrite);
			CloseHandle(hPipeRead);
			return strResult;
		}

		bool bProcessEnded = false;
		for (; !bProcessEnded;)
		{
			// Give some timeslice (50 ms), so we won't waste 100% CPU.
			bProcessEnded = WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0;

			// Even if process exited - we continue reading, if
			// there is some data available over pipe.
			for (;;)
			{
				char buf[1024];
				DWORD dwRead = 0;
				DWORD dwAvail = 0;

				if (!::PeekNamedPipe(hPipeRead, NULL, 0, NULL, &dwAvail, NULL))
					break;

				if (!dwAvail) // No data available, return
					break;

				if (!::ReadFile(hPipeRead, buf, min(sizeof(buf) - 1, dwAvail), &dwRead, NULL) || !dwRead)
					// Error, the child process might ended
					break;

				buf[dwRead] = 0;
				strResult += buf;
			}
		} //for

		CloseHandle(hPipeWrite);
		CloseHandle(hPipeRead);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return strResult;
	}

};