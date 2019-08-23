#pragma once
#include <string>
#include <vector>
#include "sqlite3.h"

using namespace std;

#define DATABASE_WHITELIST "Whitelist"
#define DATABASE_BLACKLIST "Blacklist"
#define DATABASE_PROJECT_LOG "ProjectBuildLog"
#define DATABASE_RENDER_LOG "ProjectRenderLog"
#define DATABASE_ACTIVE_LOG "ProjectActiveRenders"

namespace SQL
{
	struct ProjectSQLData
	{
		std::string ProjectID = "";
		std::string LocationID = "";
		std::string ProjectType = "";
		std::string ImageType = "";
		std::string Directory = "";
	};

	struct RenderData
	{
		std::string ProjectID = "";
		std::string LocationID = "";
		std::string ProjectType = "";
		std::string ImageType = "";
		std::string CreatedAt = "";
		std::string Status = "";
		std::string Directory = "";
		unsigned int Retries = 0;
	};

	//Add objects to the SQL table. Use overloads.
	void SQL_AddRenderLog(ProjectSQLData data, string archive, struct sqlite3* db);//Add a render log to the table

	//Add rendering info for a project render
	string SQL_AddActiveRenderLog(RenderData data, struct sqlite3* db);//Add an active render log to the table

	//Adjust rendering info for a project render
	bool SQL_AdjustActiveRenderInformation(RenderData data, struct sqlite3* db);//change the active log

	//Check if an object exists within a table
	bool SQL_ExistsWithinTable(string TableID, string ProjectID, string ImagePath, struct sqlite3* db);

	//Load a database
	bool SQL_LoadDatabase(struct sqlite3** db, std::string directory);

	struct ProjectSQLData SQL_GetProjectBuildLog(struct sqlite3* db, std::string directory);

	struct RenderData SQL_CollectActiveRenderingData(struct sqlite3* db, ProjectSQLData data);

	//Get a datetime stamp
	const std::string CurrentDateTime();

	//Replace an occurence with something else
	void FindAndReplaceAll(std::string& data, std::string toSearch, std::string replaceStr);

	namespace _CALLBACK
	{
		static int SQL_GenericCallback(void* NotUsed, int argc, char** argv, char** azColName);

		static int SQL_ExistCallback(void* Exists, int argc, char** argv, char** azColName);

		static int SQL_RenderCallback(void* Data, int argc, char** argv, char** azColName);

		static int SQL_GetProjectBuildcallback(void* Data, int argc, char** argv, char** azColName);

		static int SQL_GetActiveRenderDataCallback(void* Data, int argc, char** argv, char** azColName);
	}

}