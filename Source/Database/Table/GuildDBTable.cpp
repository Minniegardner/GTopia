#include "GuildDBTable.h"

static TableQuery sGuildQueryTable[] = {
    {"SELECT ID, Name, Statement, Notebook, WorldID, OwnerID, Level, XP, LogoFG, LogoBG, ShowMascot, CreatedAt, MemberCount, HEX(MemberData) AS MemberDataHex FROM Guilds WHERE ID = ? LIMIT 1;", QUERY_FLAG_RETURN_RESULT},
    {"SELECT ID FROM Guilds WHERE Name = ? LIMIT 1;", QUERY_FLAG_RETURN_RESULT},
    {"UPDATE Guilds SET Name = ?, Statement = ?, Notebook = ?, WorldID = ?, OwnerID = ?, Level = ?, XP = ?, LogoFG = ?, LogoBG = ?, ShowMascot = ?, CreatedAt = ?, MemberCount = ?, MemberData = UNHEX(?) WHERE ID = ?;", QUERY_FLAG_NONE},
    {"INSERT INTO Guilds (ID, Name, Statement, Notebook, WorldID, OwnerID, Level, XP, LogoFG, LogoBG, ShowMascot, CreatedAt, MemberCount, MemberData) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, UNHEX(?));", QUERY_FLAG_NONE}
};

void DatabaseGuildExec(DatabasePool* pPool, eGuildDBQuery queryID, QueryRequest& req, bool prepared)
{
    TableQuery& query = sGuildQueryTable[queryID];

    if(prepared) {
        query.flags |= QUERY_FLAG_PREPARED;
    }

    DatabaseExec(
        pPool,
        query.query,
        req,
        query.flags
    );
}

QueryRequest MakeGetGuildByIDReq(uint64 guildID, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(1);
    req.data[0] = guildID;
    return req;
}

QueryRequest MakeGetGuildByNameReq(const string& guildName, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(1);
    req.data[0] = guildName;
    return req;
}

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
)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(14);
    req.data[0] = name;
    req.data[1] = statement;
    req.data[2] = notebook;
    req.data[3] = worldID;
    req.data[4] = ownerUserID;
    req.data[5] = level;
    req.data[6] = xp;
    req.data[7] = logoFG;
    req.data[8] = logoBG;
    req.data[9] = showMascot ? 1 : 0;
    req.data[10] = createdAtMS;
    req.data[11] = memberCount;
    req.data[12] = memberDataHex;
    req.data[13] = guildID;
    return req;
}

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
)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(14);
    req.data[0] = guildID;
    req.data[1] = name;
    req.data[2] = statement;
    req.data[3] = notebook;
    req.data[4] = worldID;
    req.data[5] = ownerUserID;
    req.data[6] = level;
    req.data[7] = xp;
    req.data[8] = logoFG;
    req.data[9] = logoBG;
    req.data[10] = showMascot ? 1 : 0;
    req.data[11] = createdAtMS;
    req.data[12] = memberCount;
    req.data[13] = memberDataHex;
    return req;
}
