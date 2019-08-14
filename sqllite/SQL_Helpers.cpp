#include "SQL_Helpers.h"
#include <experimental/filesystem>
#include <iostream>
#include <thread>

using namespace std;

namespace SQL
{

	bool SQL_LoadDatabase(sqlite3** db, std::string directory)
	{
		//check if the databse file exists
		bool buildDB = !experimental::filesystem::exists(directory);

		//if it doesnt leave and report failure
		if (buildDB)
			return false;

		int rc = sqlite3_open(directory.c_str(), db);

		//if theres an error code report a failure
		if (rc)
		{
			fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(*db));
			sqlite3_close(*db);
			db = nullptr;
			return false;
		}

		return true;
	}


	struct ProjectSQLData SQL_GetProjectBuildLog(struct sqlite3* db, std::string directory)
	{
		string pSQL = "select ProjectType, ImageType, ProjectID, LocationID from " + string(DATABASE_PROJECT_LOG) + " where Directory=\"" + directory + "\";";

		ProjectSQLData data;
		char* err;

		int rc = sqlite3_exec(db, pSQL.c_str(), _CALLBACK::SQL_GetProjectBuildcallback, (void*)& data, &err);
		if (rc != SQLITE_OK)
		{
			cout << "SQL error: " << sqlite3_errmsg(db) << "\n";
			sqlite3_free(err);
		}

		return data;
	}

	void SQL_AddObjectToTable(string TableID, ProjectSQLData data, string archive, struct sqlite3* db)
	{
AttemptAddRenderLog:
		string pSQL = "insert into " + TableID + " (ProjectID, LocationID, CreatedAt, Directory, ProjectArchive, ProjectType, ImageType) values ('"
			+ data.ProjectID + "', '"
			+ data.LocationID + "', '"
			+ CurrentDateTime() + "', '"
			+ data.Directory + "', '"
			+ archive + "', '"
			+ data.ProjectType + "', '"
			+ data.ImageType + "')";

		char* err;
		int rc = sqlite3_exec(db, pSQL.c_str(), _CALLBACK::SQL_GenericCallback, nullptr, &err);

		if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED)
		{
			sqlite3_free(err);
			std::this_thread::sleep_for(std::chrono::milliseconds(2500));
			goto AttemptAddRenderLog;
		}
		else if (rc != SQLITE_OK)
		{
			cout << "SQL error: " << sqlite3_errmsg(db) << "\n";
			sqlite3_free(err);
		}
	}

	string SQL_AddActiveRenderLog(string TableID, ProjectSQLData data, struct sqlite3* db)
	{
	AttemptAddActiveLog:
		string date = CurrentDateTime();
		string pSQL = "insert into " + TableID + " (ProjectID, LocationID, CreatedAt, Directory, ProjectType, ImageType, Status) values ('"
			+ data.ProjectID + "', '"
			+ data.LocationID + "', '"
			+ date + "', '"
			+ data.Directory + "', '"
			+ data.ProjectType + "', '"
			+ data.ImageType + "', '"
			+ "RENDERING" + "')";

		char* err;
		int rc = sqlite3_exec(db, pSQL.c_str(), _CALLBACK::SQL_GenericCallback, nullptr, &err);

		if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED)
		{
			cout << "Database is busy... attempting again..." << endl;
			sqlite3_free(err);
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			goto AttemptAddActiveLog;
		}
		else if (rc != SQLITE_OK)
		{
			cout << "SQL error: " << sqlite3_errmsg(db) << "\n";
			sqlite3_free(err);
		}

		return date;
	}

	bool SQL_AdjustActiveRenderInformation(string TableID, ProjectSQLData data, string DateTime, string newFinished, struct sqlite3* db)
	{
AttemptAdjustInfo:
		string pSQL = "UPDATE " + TableID + " SET Status='" + newFinished + "' WHERE "
			+ "ProjectID=\"" + data.ProjectID + "\" AND "
			+ "LocationID=\"" + data.LocationID + "\" AND "
			+ "ProjectType=\"" + data.ProjectType + "\" AND "
			+ "ImageType=\"" + data.ImageType + "\" AND "
			+ "CreatedAt=\"" + DateTime + "\""
			+ ";";

		char* err;
		int rc = sqlite3_exec(db, pSQL.c_str(), _CALLBACK::SQL_GenericCallback, nullptr, &err);

		if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED)
		{
			sqlite3_free(err);
			std::this_thread::sleep_for(std::chrono::milliseconds(2500));
			goto AttemptAdjustInfo;
		}
		else if (rc != SQLITE_OK)
		{
			cout << "SQL error: " << sqlite3_errmsg(db) << "\n";
			sqlite3_free(err);
			return false;
		}

		return true;
	}

	bool SQL_ExistsWithinTable(string TableID, string ProjectID, string ImagePath, struct sqlite3* db)
	{
		string directory;
		string name;

		FindAndReplaceAll(ImagePath, "\\", "/");

		size_t pos;

		while ((pos = ImagePath.find("/")) != string::npos)
		{
			directory += ImagePath.substr(0, pos + 1);
			ImagePath = ImagePath.substr(pos + 1);
		}

		name = ImagePath;

		string pSQL = "SELECT * FROM " + TableID + " WHERE ProjectID=\"" + ProjectID + "\" AND Directory=\"" + directory + "\" AND Filename=\"" + name + "\";";

		int rc;
		char* err;
		bool exists = false;

		rc = sqlite3_exec(db, pSQL.c_str(), _CALLBACK::SQL_ExistCallback, (void*)&exists, &err);
		if (rc != SQLITE_OK)
		{
			cout << "SQL error: " << sqlite3_errmsg(db) << "\n";
			sqlite3_free(err);
		}

		return exists;
	}

	namespace _CALLBACK
	{

		int SQL_GenericCallback(void* NotUsed, int argc, char** argv, char** azColName)
		{
			return 0;
		}

		int SQL_ExistCallback(void* Exists, int argc, char** argv, char** azColName)
		{
			if (argc > 0)
				*((bool*)Exists) = true;
			else
				*((bool*)Exists) = false;

			return 0;
		}

		int SQL_RenderCallback(void* Exists, int argc, char** argv, char** azColName)
		{
			vector<string>* data = (vector<string>*)Exists;

			for (int i = 0; i < argc; i++)
			{
				string raw = argv[i];

				//null values mean a failed render and we dont care about those.
				if (raw == "NULL")
					continue;

				//remove avi format
				FindAndReplaceAll(raw, ".avi", "");

				//remove the directories
				size_t pos;
				while ((pos = raw.find("\\")) != string::npos)
					raw = raw.substr(pos + 1);

				//remove the project tag
				raw = raw.substr(raw.find("-") + 1);

				data->push_back(raw);
			}

			return 0;
		}

		int SQL_GetProjectBuildcallback(void* Exists, int argc, char** argv, char** azColName)
		{
			ProjectSQLData* data = (ProjectSQLData*)Exists;

			for (int i = 0; i < argc; i++)
			{
				string col = azColName[i];

				if (col == "ProjectType")
					data->ProjectType = argv[i];
				else if (col == "ImageType")
					data->ImageType = argv[i];
				else if (col == "ProjectID")
					data->ProjectID = argv[i];
				else if (col == "LocationID")
					data->LocationID = argv[i];

			}

			return 0;
		}
	}
}