#pragma once
#include <string>
#include <vector>
#include <map>

class DatabaseConnection;
class Project;
struct SQL::ProjectSQLData;
struct SQL::RenderData;

class PGSQL 
{

public:

	static void Connect(std::string ConnectionInfo, std::string ConnectionName);
	static void Disconnect(std::string ConnectionName);
	static void DisconnectAll();

	static bool ConnectionExists(std::string ConnectionName);

	static pqxx::result Query(std::string sqlQuery, std::string ConnectionName, bool DisplayQuery = false);//Raw query on a connection without any prior formatting.

	static void AddObjectToTable(std::string TableID, std::string ProjectID, std::string ImagePath, std::string ConnectionName);//Add image to blacklist/whitelist

	static void AddImageDirectory(std::string ProjectID, std::string LocationID, std::string Directory, std::string ConnectionName);//Add directory to ImageDirectories

	//static void AddProjectLog(Project* Proj, std::string ConnectionName);//Add Project entry to ProjectBuildLog

	static SQL::ProjectSQLData GetProjectBuildLog(std::string ProjectDirectory, std::string ConnectionName);

	static SQL::RenderData GetActiveRenderingLog(std::string ProjectDirectory, std::string ConnectionName);

	static bool RequiresIntegrityCheck(std::string ProjectID, std::string LocationID, std::string Directory, std::string ConnectionName);
	static void AdjustIntegrityCheck(std::string ProjectID, std::string LocationID, std::string Directory, int Integrity, std::string ConnectionName);//Adjust integrity flag in database

	static bool ExistsWithinTable(std::string TableID, std::string ProjectID, std::string ImagePath, std::string ConnectionName);//Check if an object exists within a table

	//static void GetRenderingLogs(std::vector<std::string>* Data, Project* Proj, std::string ConnectionName);
	static std::vector<std::string> GetExcludedGenericProjects(std::string ConnectionName);


private:

	static std::map<std::string, DatabaseConnection> ConnectionsMap;

	static const std::string CurrentDateTime();
	static void FindAndReplaceAll(std::string& data, std::string toSearch, std::string replaceStr);

};