#pragma once

#include "../DatabasePool.h"

static TableQuery sQueryTable[] =
{
    {"SELECT ID, HomeServerID FROM Worlds WHERE Name = ? LIMIT 1;", QUERY_FLAG_RETURN_RESULT},
    {"INSERT INTO Worlds (Name, HomeServerID, CreationDate, LastSeenTime) VALUES (?, ?, SYSDATE(), NOW());", QUERY_FLAG_RETURN_INCREMENT},
    {"SELECT ID, Name, Flags FROM Worlds WHERE Name = ?;", QUERY_FLAG_RETURN_RESULT},
    {"UPDATE Worlds SET LastSeenTime = NOW(), Name = ? WHERE ID = ?;", QUERY_FLAG_NONE},
    {"UPDATE Worlds SET HomeServerID = ? WHERE ID = ? AND HomeServerID = 0;", QUERY_FLAG_NONE}
};

enum eWorldDBQuery
{
    DB_WORLD_EXISTS_BY_NAME,
    DB_WORLD_CREATE,
    DB_WORLD_GET_DATA,
    DB_WORLD_SAVE,
    DB_WORLD_SET_HOME_SERVER
};

void DatabaseWorldExec(DatabasePool* pPool, eWorldDBQuery queryID, QueryRequest& req, bool preapred = false);

QueryRequest MakeWorldExistsByName(const string& worldName, int32 ownerID);
QueryRequest MakeWorldCreate(const string& worldName, uint32 homeServerID, int32 ownerID);
QueryRequest MakeGetWorldData(const string& worldName, int32 ownerID);
QueryRequest MakeSaveWorld(const string& worldName, uint32 worldID, int32 ownerID);
QueryRequest MakeSetWorldHomeServer(uint32 homeServerID, uint32 worldID, int32 ownerID);