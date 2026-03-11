#pragma once

#include "../DatabasePool.h"

static TableQuery sQueryTable[] =
{
    {"SELECT ID FROM Worlds WHERE Name = ? LIMIT 1;", QUERY_FLAG_RETURN_RESULT},
    {"INSERT INTO Worlds (Name, CreationDate, LastSeenTime) VALUES (?, SYSDATE(), NOW());", QUERY_FLAG_RETURN_INCREMENT},
    {"SELECT ID, Name, Flags FROM Worlds WHERE Name = ?;", QUERY_FLAG_RETURN_RESULT}
};

enum eWorldDBQuery
{
    DB_WORLD_EXISTS_BY_NAME,
    DB_WORLD_CREATE,
    DB_WORLD_GET_DATA
};

void DatabaseWorldExec(DatabasePool* pPool, eWorldDBQuery queryID, QueryRequest& req, bool preapred = false);

QueryRequest MakeWorldExistsByName(const string& worldName, int32 ownerID);
QueryRequest MakeWorldCreate(const string& worldName, int32 ownerID);
QueryRequest MakeGetWorldData(const string& worldName, int32 ownerID);