#pragma once
//#include <winsock2.h>
//#include <ws2tcpip.h>
//#include <mstcpip.h>

class DatabaseConnection
{
public:

	void SetConnection(std::string ConnectionInfo);
	void Disconnect();

	pqxx::result Query(std::string strSQL, bool DisplayText = true);

	pqxx::result LatestResult;

private:

	pqxx::connection* pgsqlConnection = nullptr;

};

