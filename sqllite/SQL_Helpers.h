#pragma once
#include <string>
#include <vector>
#include "sqlite3.h"
#include "../Helpers.h"

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
		std::string ProjectType = "";
		std::string ImageType = "";
		std::string Directory = "";
	};

	//Add objects to the SQL table. Use overloads.
	void SQL_AddObjectToTable(string TableID, string projID, string projType, string imageType, string directory, string archive, struct sqlite3* db);//Add a render log to the table

	//Add objects to the SQL table. Use overloads.
	string SQL_AddActiveRenderLog(string TableID, string projID, string projType, string imageType, string directory, struct sqlite3* db);//Add an active render log to the table

	bool SQL_AdjustActiveRenderInformation(string TableID, string projID, string projType, string imageType, string DateTime, string newFinished, struct sqlite3* db);//change the active log

	//Check if an object exists within a table
	bool SQL_ExistsWithinTable(string TableID, string ProjectID, string ImagePath, struct sqlite3* db);

	//Load a database
	bool SQL_LoadDatabase(struct sqlite3** db, std::string directory);

	struct ProjectSQLData SQL_GetProjectBuildLog(struct sqlite3* db, std::string directory);

	namespace _CALLBACK
	{
		static int SQL_GenericCallback(void* NotUsed, int argc, char** argv, char** azColName);

		static int SQL_ExistCallback(void* Exists, int argc, char** argv, char** azColName);

		static int SQL_RenderCallback(void* Data, int argc, char** argv, char** azColName);

		static int SQL_GetProjectBuildcallback(void* Data, int argc, char** argv, char** azColName);
	}
}