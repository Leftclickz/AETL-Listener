//pqxx header
#include "ListenerPCH.h"
#include <iostream>
#include <time.h>
#include <ctime>


#define DATABASE_WHITELIST "Whitelist"
#define DATABASE_IMAGE_DIRECTORIES "ImageDirectories"
#define DATABASE_BLACKLIST "Blacklist"
#define DATABASE_PROJECT_LOG "ProjectBuildLog"

#define PROJECT_FIELD_PROJECTID "\"ProjectID\""
#define PROJECT_FIELD_LOCATIONID "\"LocationID\""
#define PROJECT_FIELD_CREATEDAT "\"CreatedAt\""
#define PROJECT_FIELD_UPDATEDAT "\"UpdatedAt\""
#define PROJECT_FIELD_NAME "\"Name\""
#define PROJECT_FIELD_IMAGETYPE "\"ImageType\""
#define PROJECT_FIELD_PROJECTBUILT "\"ProjectBuilt\""
#define PROJECT_FIELD_VIDEORENDERED "\"VideoRendered\""
#define PROJECT_FIELD_UPLOADED "\"Uploaded\""
#define PROJECT_FIELD_STATUS "\"Status\""
#define PROJECT_FIELD_RETRIES "\"Retries\""

#define PGSQL_RETURN_IF_EXISTS(_ID_) if (ConnectionExists(_ID_) == true) return;
#define PGSQL_RETURN_IF_NOEXIST(_ID_) if (ConnectionExists(_ID_) == false) return;

std::map<std::string, DatabaseConnection> PGSQL::ConnectionsMap;

void PGSQL::Connect(std::string ConnectionInfo, std::string ConnectionName)
{
	PGSQL_RETURN_IF_EXISTS(ConnectionName);

	ConnectionsMap[ConnectionName].SetConnection(ConnectionInfo);
}

void PGSQL::Disconnect(std::string ConnectionName)
{
	PGSQL_RETURN_IF_NOEXIST(ConnectionName);
	ConnectionsMap[ConnectionName].Disconnect();
}

void PGSQL::DisconnectAll()
{
	for (auto const& x : ConnectionsMap)
	{
		Disconnect(x.first);
	}

	ConnectionsMap.clear();
}

bool PGSQL::ConnectionExists(std::string ConnectionName)
{
	if (ConnectionsMap.find(ConnectionName) == ConnectionsMap.end()) 
	{
		return false;
	}
	else 
	{
		return true;
	}
	
}

pqxx::result PGSQL::Query(std::string sqlQuery, std::string ConnectionName, bool DisplayQuery)
{
	pqxx::result res;

	if (ConnectionExists(ConnectionName) == false)
	{
		return res;
	}

	try 
	{
		res = ConnectionsMap[ConnectionName].Query(sqlQuery, DisplayQuery);
	}
	catch (const std::exception & e)
	{
		//error reason
		std::cerr << e.what() << std::endl;
	}


	//std::this_thread::sleep_for(std::chrono::milliseconds(100));
	return res;
}

void PGSQL::AddObjectToTable(std::string TableID, std::string LocationID, std::string ImagePath, std::string ConnectionName)
{
	PGSQL_RETURN_IF_NOEXIST(ConnectionName);

	std::string directory;
	std::string name;
	std::string pSQL;

	name = ImagePath;
	pSQL = "insert into public.\"" + TableID + "\" values ('" + LocationID + "', '" + CurrentDateTime() + "', '" + name + "')";

	(void)ConnectionsMap[ConnectionName].Query(pSQL, false);
}

void PGSQL::AddImageDirectory(std::string ProjectID, std::string LocationID, std::string Directory, std::string ConnectionName)
{
	std::string pSQL = "INSERT INTO public.\"ImageDirectories\" (\"ProjectID\", \"LocationID\", \"CreatedAt\", \"Directory\", \"RequiresIntegrityCheck\") SELECT '"
		+ ProjectID + "', '"
		+ LocationID + "', '"
		+ CurrentDateTime() + "', '"
		+ Directory + "', '"
		+ "1'"
		+ " WHERE NOT EXISTS (SELECT 1 FROM public.\"ImageDirectories\" WHERE \"Directory\"='" + Directory + "');";

	(void)ConnectionsMap[ConnectionName].Query(pSQL, false);
}

SQL::ProjectSQLData PGSQL::GetProjectBuildLog(std::string ProjectDirectory, std::string ConnectionName)
{
	//Create the container
	SQL::ProjectSQLData data;

	if (ConnectionExists(ConnectionName) == false)
	{
		return data;
	}

	//save the directory
	data.Directory = ProjectDirectory;

	//Strip the directory so we see which project we're fetching.
	FindAndReplaceAll(ProjectDirectory, ".aep", "");
	FindAndReplaceAll(ProjectDirectory, Dir::HotFolder + "\\", "");

	if (ProjectDirectory.find("MONTHLY") != std::string::npos)
	{
		data.ProjectType = "MONTHLY";
	}
	else
	{
		data.ProjectType = "GENERIC";
	}

	data.ProjectID = ProjectDirectory.substr(0, ProjectDirectory.find('-'));
	data.LocationID = ProjectDirectory.substr(ProjectDirectory.find('-') + 1, 2);


	//We can figure out a lot from just the project name alone but we need to find out the image type and thats stored in the database.
	std::string pSQL = "SELECT * FROM public.\"ProjectBuildLog\" WHERE \"Name\"='" + ProjectDirectory + "';";
	pqxx::result res = ConnectionsMap[ConnectionName].Query(pSQL, false);

	//This should literally never happen but we can still salvage the operation with some defaults
	//It shouldnt happen since every project file should have a corresponding entry in the database.
	if (res.size() == 0)
	{
		data.ImageType = "NORMAL";
		return data;
	}

	data.ImageType = res[0]["\"ImageType\""].c_str();
	return data;
}

SQL::RenderData PGSQL::GetActiveRenderingLog(std::string ProjectDirectory, std::string ConnectionName)
{
	SQL::RenderData data;

	if (ConnectionExists(ConnectionName) == false)
	{
		return data;
	}

	//Copy basic data
	data.ImageType = Project::SQL_DATA.ImageType;
	data.Directory = Project::SQL_DATA.Directory;
	data.LocationID = Project::SQL_DATA.LocationID;
	data.ProjectID = Project::SQL_DATA.ProjectID;
	data.ProjectType = Project::SQL_DATA.ProjectType;
	data.Status = "INCOMPLETE";

	//Get our info again
	std::string pSQL = "SELECT * FROM public.\"ProjectBuildLog\" WHERE \"Name\"='" + Project::PROJECT_NAME + "';";
	pqxx::result res = ConnectionsMap[ConnectionName].Query(pSQL, false);

	if (res.size() == 0)
	{
		data.Retries = 0;
		data.CreatedAt = CurrentDateTime();
		return data;
	}

	data.Retries = atoi(res[0]["\"Retries\""].c_str());
	data.CreatedAt = res[0]["\"CreatedAt\""].c_str();

	if (data.Retries > 0)
	{
		data.Status = "RETRY";
	}

	return data;
}

//void PGSQL::AddProjectLog(Project* Proj, std::string ConnectionName)
//{
//	std::string projectID = Proj->GetProjectID();
//	std::string locationID = Proj->GetLocationID();
//	std::string name = Proj->GetProjectVideoName();
//	std::string type = "";
//	std::string buildTime = CurrentDateTime();
//	std::string latestDL = Proj->GetLatestDownloadTimestamp();
//	std::string latestDLFilename = Proj->GetLatestDownloadFilename();
//	std::string buildTimestamp = Proj->AdobeProjectGenerated() ? "'" + buildTime + "'" : "NULL";
//
//	switch (Proj->GetProjectMode())
//	{
//	case FAIL:
//		type = "NORMAL";
//		break;
//	case DEFAULT_PROJ_MODE:
//		type = "NORMAL";
//		break;
//	case ALIGN:
//		type = "ALIGN";
//		break;
//	case NORMAL:
//		type = "NORMAL";
//		break;
//	default:
//		type = "NORMAL";
//		break;
//	}
//
//	std::string pSQL = "INSERT INTO public.\"" + std::string(DATABASE_PROJECT_LOG) + "\" VALUES ("
//		+ projectID + ", " + locationID + ", '" + buildTime + "', '" + buildTime + "', '" + name + "', '" + type + "', " + buildTimestamp + ", NULL, NULL, 'INCOMPLETE', 0, '" + latestDL + "', '" + latestDLFilename  + "')"
//		+ " ON CONFLICT (\"Name\") DO UPDATE SET " +
//		" \"UpdatedAt\"=EXCLUDED.\"UpdatedAt\", \"ProjectBuilt\"=EXCLUDED.\"ProjectBuilt\", \"VideoRendered\"=EXCLUDED.\"VideoRendered\", \"Uploaded\"=EXCLUDED.\"Uploaded\", \"Status\"=EXCLUDED.\"Status\", \"Retries\"=0";
//
//	//Add the lastest download to the update command IF something was actually updated.
//	if (latestDL != "")
//	{
//		pSQL += ", \"LastDownloadTimestamp\"='" + latestDL + "', \"LastDownload\"='" + latestDLFilename + "';";
//	}
//	else
//	{
//		pSQL += ";";
//	}
//
//	(void)ConnectionsMap[ConnectionName].Query(pSQL, false);
//}

bool PGSQL::RequiresIntegrityCheck(std::string ProjectID, std::string LocationID, std::string Directory, std::string ConnectionName)
{
	if (ConnectionExists(ConnectionName) == false) return false;

	std::string pSQL = "SELECT \"RequiresIntegrityCheck\" FROM public.\"" + std::string(DATABASE_IMAGE_DIRECTORIES) + "\" WHERE \"ProjectID\"="
		+ ProjectID + " AND \"LocationID\"=" + LocationID + " AND \"Directory\"='" + Directory + "';";

	pqxx::result res = ConnectionsMap[ConnectionName].Query(pSQL, false);
	return res[0]["\"RequiresIntegrityCheck\""].as<bool>();
}

void PGSQL::AdjustIntegrityCheck(std::string ProjectID, std::string LocationID, std::string Directory, int Integrity, std::string ConnectionName)
{
	PGSQL_RETURN_IF_NOEXIST(ConnectionName);

	std::string pSQL = "UPDATE public.\"" + std::string(DATABASE_IMAGE_DIRECTORIES) + "\" SET \"RequiresIntegrityCheck\"='" + std::to_string(Integrity) + "' WHERE "
		+ "\"ProjectID\"=" + ProjectID + " AND "
		+ "\"LocationID\"=" + LocationID + " AND "
		+ "\"Directory\"='" + Directory + "'"
		+ ";";

	(void)ConnectionsMap[ConnectionName].Query(pSQL, false);
}

bool PGSQL::ExistsWithinTable(std::string TableID, std::string ProjectID, std::string ImagePath, std::string ConnectionName)
{
	if (ConnectionExists(ConnectionName) == false) return false;

	std::string pSQL = "SELECT * FROM public.\"" + TableID + "\" WHERE \"ProjectID\"=" + ProjectID + " AND \"Filename\"='" + ImagePath + "';";

	pqxx::result res = ConnectionsMap[ConnectionName].Query(pSQL, false);

	if (res.size() == 0)
		return false;
	else
		return true;
}

//void PGSQL::GetRenderingLogs(std::vector<std::string>* Data, Project* Proj, std::string ConnectionName)
//{
//	PGSQL_RETURN_IF_NOEXIST(ConnectionName);
//
//	std::string pSQL = "SELECT * FROM public.\"" + std::string(DATABASE_PROJECT_LOG) + "\" WHERE \"LocationID\"=" + Proj->GetLocationID() + ";";
//
//	pqxx::result res = ConnectionsMap[ConnectionName].Query(pSQL, false);
//
//	//lambda to add data to the vector to avoid duplicating code
//	auto add = [](std::vector<std::string>* data, std::string name) 
//	{
//		size_t loc = name.find("MONTHLY") + 8;
//		std::string infoToAdd = name.substr(loc, loc + 7);
//
//		data->push_back(infoToAdd);
//	};
//
//	for (pqxx::result_size_type i = 0; i < res.size(); i++)
//	{
//		std::string projectName = res[i][PROJECT_FIELD_NAME].c_str();
//		std::string status = res[i][PROJECT_FIELD_STATUS].c_str();
//
//		bool isMonthly = projectName.find("MONTHLY") != std::string::npos;
//
//		//Monthly projects are never uploaded more than once so we need to ensure that if a project is in transit already or has been completed we don't make another.
//		if (isMonthly)
//		{
//			//Something went wrong so we're restarting this project.
//			if (status == "ERROR")
//			{	
//				continue;
//			}
//			//This project is still in progress. We still need to check the data though in case something unexpectedly stalled.
//			else if (status == "INCOMPLETE")
//			{
//				//Has the project not been rendered as video yet?
//				if (res[i][PROJECT_FIELD_VIDEORENDERED].is_null())
//				{
//					//If the project is null too then something clearly went wrong the last time the generator ran so we need to build this again.
//					if (res[i][PROJECT_FIELD_PROJECTBUILT].is_null())
//					{
//						continue;
//					}
//
//					//if the project was made then we need to check when because if it's been 2 days and the video hasnt been rendered it probably broke somehow
//					std::string projectBuildDate = res[i][PROJECT_FIELD_PROJECTBUILT].c_str();
//					
//					//We'll use epoch time to check.
//					tm buildDateTimeStruct;
//					buildDateTimeStruct.tm_year = atoi(projectBuildDate.substr(0, 4).c_str()) - 1900;
//					buildDateTimeStruct.tm_mon = atoi(projectBuildDate.substr(5, 2).c_str()) - 1;
//					buildDateTimeStruct.tm_mday = atoi(projectBuildDate.substr(8, 2).c_str());
//					buildDateTimeStruct.tm_hour = atoi(projectBuildDate.substr(11, 2).c_str());
//					buildDateTimeStruct.tm_min = atoi(projectBuildDate.substr(14, 2).c_str());
//					buildDateTimeStruct.tm_sec = atoi(projectBuildDate.substr(17, 2).c_str());
//					buildDateTimeStruct.tm_isdst = -1;
//
//					time_t buildDateAsEpoch = mktime(&buildDateTimeStruct);
//					time_t dateAsEpoch = std::time(nullptr);
//
//					//Convert seconds to days 60 * 60 * 24
//					float daysSince = (float)((dateAsEpoch - buildDateAsEpoch) / 8640.f);
//
//					//If it's been less than 2 days then we can ignore it.
//					if (daysSince < 2.0f)
//					{
//						add(Data, projectName);
//						continue;
//					}
//				}
//				//If it's been rendered but is incomplete that means the uploader hasn't finished yet. it can be ignored.
//				else
//				{
//					add(Data, projectName);
//				}
//			}
//			//If the status isnt incomplete or error then it's complete which means it's good and can be ignored.
//			else
//			{
//				add(Data, projectName);
//			}
//		}
//	}
//}

std::vector<std::string> PGSQL::GetExcludedGenericProjects(std::string ConnectionName)
{
	std::string pSQL = "SELECT * FROM public.\"" + std::string(DATABASE_PROJECT_LOG) + "\";";

	pqxx::result res = ConnectionsMap[ConnectionName].Query(pSQL, false);

	std::vector<std::string> excludedProjects;

	for (pqxx::result::size_type i = 0; i < res.size(); i++)
	{
		std::string projectName = res[i][PROJECT_FIELD_NAME].c_str();
		std::string status = res[i][PROJECT_FIELD_STATUS].c_str();

		bool isMonthly = projectName.find("MONTHLY") != std::string::npos;

		//Ignore all monthly results
		if (isMonthly)
		{
			continue;
		}

		//Something went wrong so we're restarting this project.
		if (status == "ERROR")
		{
			continue;
		}
		//This project is still in progress. We still need to check the data though in case something unexpectedly stalled.
		else if (status == "INCOMPLETE")
		{
			//Has the project not been rendered as video yet?
			if (res[i][PROJECT_FIELD_VIDEORENDERED].is_null())
			{
				//If the project is null too then something clearly went wrong the last time the generator ran so we need to build this again.
				if (res[i][PROJECT_FIELD_PROJECTBUILT].is_null())
				{
					continue;
				}

				//if the project was made then we need to check when because if it's been 2 days and the video hasnt been rendered it probably broke somehow
				std::string projectBuildDate = res[i][PROJECT_FIELD_PROJECTBUILT].c_str();

				//We'll use epoch time to check.
				tm buildDateTimeStruct;
				buildDateTimeStruct.tm_year = atoi(projectBuildDate.substr(0, 4).c_str()) - 1900;
				buildDateTimeStruct.tm_mon = atoi(projectBuildDate.substr(5, 2).c_str()) - 1;
				buildDateTimeStruct.tm_mday = atoi(projectBuildDate.substr(8, 2).c_str());
				buildDateTimeStruct.tm_hour = atoi(projectBuildDate.substr(11, 2).c_str());
				buildDateTimeStruct.tm_min = atoi(projectBuildDate.substr(14, 2).c_str());
				buildDateTimeStruct.tm_sec = atoi(projectBuildDate.substr(17, 2).c_str());
				buildDateTimeStruct.tm_isdst = -1;

				time_t buildDateAsEpoch = mktime(&buildDateTimeStruct);
				time_t dateAsEpoch = std::time(nullptr);

				//Convert seconds to days 60 * 60 * 24
				float daysSince = (float)((dateAsEpoch - buildDateAsEpoch) / 8640.f);

				//If it's been less than 2 days then we can ignore it.
				if (daysSince < 2.0f)
				{
					excludedProjects.push_back(projectName);
					continue;
				}
			}
			//If it's been rendered but is incomplete that means the uploader hasn't finished yet. it can be ignored.
			else
			{
				excludedProjects.push_back(projectName);
				continue;
			}
		}
		//If the status isnt incomplete or error then it's complete which means we can generate a new one.
		else
		{
			continue;
		}
	}

	return excludedProjects;
}

const std::string PGSQL::CurrentDateTime()
{
	time_t     now = time(0);
	struct tm  tstruct;
	char       buf[80];
	tstruct = *localtime(&now);

	strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);

	std::string data(buf);

	return data;
}

void PGSQL::FindAndReplaceAll(std::string& data, std::string toSearch, std::string replaceStr)
{
	// Get the first occurrence
	size_t pos = data.find(toSearch);

	// Repeat till end is reached
	while (pos != std::string::npos)
	{
		// Replace this occurrence of Sub std::string
		data.replace(pos, toSearch.size(), replaceStr);
		// Get the next occurrence from the current position
		pos = data.find(toSearch, pos + replaceStr.size());
	}
}


