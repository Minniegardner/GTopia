#pragma once

#include "Database/DatabasePool.h"

enum eGuildDBQuery {
    DB_GUILD_GET_BY_ID,
    DB_GUILD_GET_BY_NAME,
    DB_GUILD_UPDATE,
    DB_GUILD_INSERT
};

void DatabaseGuildExec(DatabasePool* pPool, eGuildDBQuery queryID, QueryRequest& req, bool prepared = false);

QueryRequest MakeGetGuildByIDReq(uint64 guildID, int32 ownerID);
QueryRequest MakeGetGuildByNameReq(const string& guildName, int32 ownerID);
QueryRequest MakeUpdateGuildReq(
    uint64 guildID,
    const string& name,
    const string& statement,
    const string& notebook,
    uint32 worldID,
    uint32 ownerUserID,
    uint32 level,
    uint32 xp,
    uint16 logoFG,
    uint16 logoBG,
    bool showMascot,
    uint64 createdAtMS,
    uint32 memberCount,
    const string& memberDataHex,
    int32 ownerID
);
QueryRequest MakeInsertGuildReq(
    uint64 guildID,
    const string& name,
    const string& statement,
    const string& notebook,
    uint32 worldID,
    uint32 ownerUserID,
    uint32 level,
    uint32 xp,
    uint16 logoFG,
    uint16 logoBG,
    bool showMascot,
    uint64 createdAtMS,
    uint32 memberCount,
    const string& memberDataHex,
    int32 ownerID
);
