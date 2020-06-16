#include "ListenerPCH.h"
#include <iostream>

void DatabaseConnection::SetConnection(std::string ConnectionInfo)
{
	if (pgsqlConnection != nullptr)
	{
		Disconnect();
	}

	//if (ConnectionInfo == "")
	//{
	//	pgsqlConnection = new pqxx::connection(
	//		"host=10.20.2.6 "
	//		"dbname=postgres "
	//		"port=5432 "
	//		"user=admin@ceecam.net "
	//		"password=c33c@m!!");
	//}



	pgsqlConnection = new pqxx::connection(ConnectionInfo);
}

void DatabaseConnection::Disconnect()
{
	if (pgsqlConnection != nullptr)
	{
		pgsqlConnection->disconnect();
		delete pgsqlConnection;
		pgsqlConnection = nullptr;
	}
}

pqxx::result DatabaseConnection::Query(std::string strSQL, bool DisplayText)
{
	if (DisplayText)
		printf("Processing query command \"%s\"\n", strSQL.c_str());
	pqxx::result res;

	try 
	{
		pqxx::work trans(*pgsqlConnection, "trans");
		res = trans.exec(strSQL);
		trans.commit();
		LatestResult = res;
	}
	catch (const std::exception & e)
	{
		//error reason
		std::cerr << e.what() << std::endl;

		LogFile::WriteToLog("Error upon attempting connection: " + std::string(e.what()));
	}


	return res;
}
