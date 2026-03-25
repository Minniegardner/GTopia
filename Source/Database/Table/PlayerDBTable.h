#pragma once

#include "../DatabasePool.h"

static TableQuery sQueryTable[] =
{
    {"SELECT ID FROM Players WHERE Name = '' AND Mac = ? AND RID = UNHEX(?) AND PlatformType = ? LIMIT 1;", QUERY_FLAG_RETURN_RESULT},
    {"INSERT INTO Players (GuestName, PlatformType, GuestID, Mac, RID, CreationDate, LastSeenTime) VALUES (?, ?, ?, ?, UNHEX(?), SYSDATE(), NOW());", QUERY_FLAG_RETURN_INCREMENT},
    {"SELECT GuestID, RoleID, Inventory FROM Players WHERE ID = ?;", QUERY_FLAG_RETURN_RESULT},
    {"UPDATE Players SET LastSeenTime = NOW(), RoleID = ?, Inventory = ? WHERE ID = ?;", QUERY_FLAG_NONE},
    {"SELECT ID, RoleID, Name, GuestName, Password, GuestID FROM Players WHERE ID = ?;", QUERY_FLAG_RETURN_RESULT}
};

enum ePlayerDBQuery
{
    DB_PLAYER_GET_BY_MAC,
    DB_PLAYER_CREATE,
    DB_PLAYER_GET_DATA,
    DB_PLAYER_SAVE,
    DB_PLAYER_GET_FOR_INACTIVE
};

void DatabasePlayerExec(DatabasePool* pPool, ePlayerDBQuery queryID, QueryRequest& req, bool preapred = false);

QueryRequest MakePlayerByMacReq(const string& mac, const string& rid, uint8 platformType, int32 ownerID);
QueryRequest MakePlayerCreateReq(const string& guestName, uint8 platformType, uint16 guestID, const string& mac, const string& rid, int32 ownerID);
QueryRequest MakeGetPlayerDataReq(uint32 userID, int32 ownerID);
QueryRequest MakeSavePlayerReq(uint32 userID, uint32 roleID, const string& inventoryData, int32 ownerID);
QueryRequest MakeGetForInactive(uint32 userID, int32 ownerID);