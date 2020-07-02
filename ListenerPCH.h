#pragma once

#if _DEBUG
#pragma comment(lib, "libs/pqxx.lib")
#else
#pragma comment(lib, "libs/release/pqxx.lib")
#endif
#pragma comment(lib, "Ws2_32.lib")

//internet connection library
#pragma comment(lib,"Wininet.lib")

//libpq
#include <pqxx/pqxx>

#include <atlstr.h>

//personal files
#include "AETL_Uploading.h"
#include "Helpers.h"
#include "LogFile.h"
#include "postgres/DatabaseConnection.h"
#include "postgres/PGSQL_Helpers.h"
#include "kguithread.h"

//not supported but included anyway so nothing breaks
#include "sqllite/SQL_Helpers.h"
#include "sqllite/sqlite3.h"
