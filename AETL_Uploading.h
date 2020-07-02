#pragma once

#include <filesystem>

#define DEFAULT_CURL_COMMAND std::string("curl -X PUT -H \"Authorization: " + DEFAULT_TEST_AUTH + "\" -F \"resolution=720p\" -F \"type=MONTHLY\" -F \"year=2019\" -F \"month=11\" -F \"source_file=@MyFile.mp4\" https://ceeplayer.net/v1/locations/88/timelapses/upload/")
#define DEFAULT_TEST_COMMAND std::string("curl -i -X PUT -H \"Authorization: " + DEFAULT_TEST_AUTH + "\" -F \"resolution=720p\" -F \"type=MONTHLY\" -F \"year=2019\" -F \"month=11\" -F \"source_file=@MyFile.mp4\" https://ceeplayer-testing.ulam.io/v1/locations/1965/timelapses/upload/")
#define DEFAULT_RES "resolution=720p"
#define DEFAULT_TYPE "type=MONTHLY"
#define DEFAULT_YEAR "year=2019"
#define DEFAULT_MONTH "month=11"
#define DEFAULT_FILENAME "source_file=@MyFile.mp4"
#define DEFAULT_URL "https://ceeplayer.net/v1/locations/88/timelapses/upload/"
#define DEFAULT_LOCATION_ID "88"
#define DEFAULT_CURL_PATH "curl"
#define DEFAULT_TEST_AUTH std::string("NYEZAtUJMD1rZ4jAtCgDXFBIO7F4n072kJKDavd5btw")

//where curl should be located.
#define CURL_EXE std::string("curl\\curl.exe")

//this covers both 200 and 201 which are successful response codes
#define CLEAN_RESPONSE_CODE "HTTP/2 20"
#define ERROR_BAD_HTTP_RESPONSE -1

namespace AETL_Upload {

	struct UploadParams
	{
		std::string Type = "type=";
		std::string Res = "resolution=";
		std::string Month = "month=";
		std::string Year = "year=";
		std::string File = "source_file=@";
		std::string URL = DEFAULT_URL;
	};


	enum class UploadResponseCodes
	{
		SUCCESS = 0,
		FAILED_UPLOAD_FAILED = 1,
		FAILED_FILE_NOT_DELETED = 2,
		FAILED_CURL_NOT_INSTALLED = 3
	};

	//The upload command to send data to the endpoint
	UploadResponseCodes UploadUsingCurl(std::filesystem::path Filepath, std::string AuthKey);

	//Raw code execution
	CStringA ExecCmd(const wchar_t* cmd);
};