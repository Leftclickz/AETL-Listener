#include "SQL_Helpers.h"
#include <filesystem>
#include <iostream>
#include <thread>

using namespace std;

namespace SQL
{

	bool SQL_LoadDatabase(sqlite3** db, std::string directory)
	{
		//check if the databse file exists
		bool buildDB = !filesystem::exists(directory);

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
		ProjectSQLData data;
		string pSQL = "select ProjectType, ImageType, ProjectID, LocationID from " + string(DATABASE_PROJECT_LOG) + " where Directory=\"" + directory + "\";";
		char* err;
		int rc;
		do 
		{
			rc = sqlite3_exec(db, pSQL.c_str(), _CALLBACK::SQL_GetProjectBuildcallback, (void*)& data, &err);

			if (rc != SQLITE_OK)
			{
				cout << "SQL error: " << sqlite3_errmsg(db) << "\n";
				sqlite3_free(err);
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}

		} while (rc != SQLITE_OK);


		return data;
	}

	struct RenderData SQL_CollectActiveRenderingData(struct sqlite3* db, ProjectSQLData data)
	{
		RenderData renderData;
		string pSQL = "SELECT * FROM " + string(DATABASE_ACTIVE_LOG) + " WHERE "
			+ "ProjectID=\"" + data.ProjectID + "\" AND "
			+ "LocationID=\"" + data.LocationID + "\" AND "
			+ "ProjectType=\"" + data.ProjectType + "\" AND "
			+ "ImageType=\"" + data.ImageType + "\" AND "
			+ "Status=\"" + "RETRY" + "\";";

		int rc;
		char* err;

		do 
		{
			rc = sqlite3_exec(db, pSQL.c_str(), _CALLBACK::SQL_GetActiveRenderDataCallback, (void*)& renderData, &err);
			if (rc != SQLITE_OK)
			{
				cout << "SQL error: " << sqlite3_errmsg(db) << "\n";
				sqlite3_free(err);
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}
		} while (rc != SQLITE_OK);

		return renderData;
	}

	void SQL_AddRenderLog(ProjectSQLData data, string archive, struct sqlite3* db)
	{
		string pSQL = "insert into " + string(DATABASE_RENDER_LOG) + " (ProjectID, LocationID, CreatedAt, Directory, ProjectArchive, ProjectType, ImageType) values ('"
			+ data.ProjectID + "', '"
			+ data.LocationID + "', '"
			+ CurrentDateTime() + "', '"
			+ data.Directory + "', '"
			+ archive + "', '"
			+ data.ProjectType + "', '"
			+ data.ImageType + "')";

		char* err;
		int rc;

		do 
		{
			rc = sqlite3_exec(db, pSQL.c_str(), _CALLBACK::SQL_GetActiveRenderDataCallback, nullptr, &err);
			if (rc != SQLITE_OK)
			{
				cout << "SQL error: " << sqlite3_errmsg(db) << "\n";
				sqlite3_free(err);
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}
		} while (rc != SQLITE_OK);
	}

	string SQL_AddActiveRenderLog(RenderData data, struct sqlite3* db)
	{
		string date = CurrentDateTime();
		string pSQL = "insert into " + string(DATABASE_ACTIVE_LOG) + " (ProjectID, LocationID, CreatedAt, Directory, ProjectType, ImageType, Status, Retries) values ('"
			+ data.ProjectID + "', '"
			+ data.LocationID + "', '"
			+ date + "', '"
			+ data.Directory + "', '"
			+ data.ProjectType + "', '"
			+ data.ImageType + "', '"
			+ data.Status + "', '"
			+ to_string(data.Retries) + "')";
		char* err;
		int rc;

		do 
		{
			rc = sqlite3_exec(db, pSQL.c_str(), _CALLBACK::SQL_GenericCallback, nullptr, &err);
			if (rc != SQLITE_OK)
			{
				cout << "SQL error: " << sqlite3_errmsg(db) << "\n";
				sqlite3_free(err);
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}
		} while (rc != SQLITE_OK);

		return date;
	}

	bool SQL_AdjustActiveRenderInformation(RenderData data, struct sqlite3* db)
	{
		string pSQL = "UPDATE " + string(DATABASE_ACTIVE_LOG) + " SET Status='" + data.Status + "', Retries='" + to_string(data.Retries) + "' WHERE "
			+ "ProjectID=\"" + data.ProjectID + "\" AND "
			+ "LocationID=\"" + data.LocationID + "\" AND "
			+ "ProjectType=\"" + data.ProjectType + "\" AND "
			+ "ImageType=\"" + data.ImageType + "\" AND "
			+ "CreatedAt=\"" + data.CreatedAt + "\""
			+ ";";

		char* err;
		int rc;
		
		do 
		{
			rc = sqlite3_exec(db, pSQL.c_str(), _CALLBACK::SQL_GenericCallback, nullptr, &err);

			if (rc != SQLITE_OK)
			{
				cout << "SQL error: " << sqlite3_errmsg(db) << "\n";
				sqlite3_free(err);
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}
		} while (rc != SQLITE_OK);

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

		do {
			rc = sqlite3_exec(db, pSQL.c_str(), _CALLBACK::SQL_ExistCallback, (void*)& exists, &err);
			if (rc != SQLITE_OK)
			{
				cout << "SQL error: " << sqlite3_errmsg(db) << "\n";
				sqlite3_free(err);
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}
		} while (rc != SQLITE_OK);

		return exists;
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

		int SQL_GetActiveRenderDataCallback(void* Data, int argc, char** argv, char** azColName)
		{
			RenderData* data = (RenderData*)Data;

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
				else if (col == "Status")
					data->Status = argv[i];
				else if (col == "Retries")
					data->Retries = atoi(argv[i]);
				else if (col == "Directory")
					data->Directory = argv[i];
				else if (col == "CreatedAt")
					data->CreatedAt = argv[i];
			}

			return 0;
		}

	}
}