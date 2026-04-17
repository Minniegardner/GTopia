#include "PlayerDBTable.h"

    {"UPDATE Players SET Name = ?, Password = UNHEX(MD5(?)) WHERE ID = ?;"},
    {"SELECT ID FROM Players WHERE Name = ? AND Password = UNHEX(MD5(?));", QUERY_FLAG_RETURN_RESULT},
    {"UPDATE Players SET Name = ? WHERE ID = ?;"}
    TableQuery& query = sQueryTable[queryID];

    if(preapred) {
        query.flags |= QUERY_FLAG_PREPARED;
    }

    DatabaseExec(
        pPool,
        query.query,
        req,
        query.flags
    );
}

QueryRequest MakePlayerByMacReq(const string& mac, uint8 platformType, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(2);
    req.data[0] = mac;
    req.data[1] = platformType;
    return req;
}

QueryRequest MakePlayerCreateReq(const string& guestName, uint8 platformType, uint16 guestID, const string& mac, const string& ip, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(5);
    req.data[0] = guestName;
    req.data[1] = platformType;
    req.data[2] = guestID;
    req.data[3] = mac;
    req.data[4] = ip;
    return req;
}

QueryRequest MakeGetPlayerDataReq(uint32 userID, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(1);
    req.data[0] = userID;
    return req;
}

QueryRequest MakeSavePlayerReq(uint32 userID, uint32 roleID, const string& inventoryData, int32 gems, uint32 level, uint32 xp, const string& achievementData, const string& statisticData, uint32 dailyRewardStreak, uint32 dailyRewardClaimDay, uint64 lastClaimDailyRewardMs, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(11);
    req.data[0] = roleID;
    req.data[1] = inventoryData,
    req.data[2] = gems,
    req.data[3] = level,
    req.data[4] = xp,
    req.data[5] = achievementData;
    req.data[6] = statisticData;
    req.data[7] = dailyRewardStreak;
    req.data[8] = dailyRewardClaimDay;
    req.data[9] = lastClaimDailyRewardMs;
    req.data[10] = userID;
    return req;
}

QueryRequest MakeGetForInactive(uint32 userID, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(1);
    req.data[0] = userID;
    return req;
}

QueryRequest MakeCountCreatedAccByIP(const string& ip, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(1);
    req.data[0] = ip;
    return req;
}

QueryRequest MakePlayerByVIDReq(const string& vid, uint8 platformType, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(2);
    req.data[0] = vid;
    req.data[1] = platformType;
    return req;
}

QueryRequest MakePlayerByGIDReq(const string& gid, uint8 platformType, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(2);
    req.data[0] = gid;
    req.data[1] = platformType;
    return req;
}

QueryRequest MakePlayerByHashReq(int32 hash, uint8 platformType, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(2);
    req.data[0] = hash;
    req.data[1] = platformType;
    return req;
}

QueryRequest MakeCountPlayerByGidMacIP(const string& gid, const string& mac, const string& ip, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(3);
    req.data[0] = gid;
    req.data[1] = mac;
    req.data[2] = ip;
    return req;
}

QueryRequest MakeCountPlayerByVidMacIP(const string& vid, const string& mac, const string& ip, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(3);
    req.data[0] = vid;
    req.data[1] = mac;
    req.data[2] = ip;
    return req;
}

QueryRequest MakeCountPlayerBySidMacIP(const string& sid, const string& mac, const string& ip, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(3);
    req.data[0] = sid;
    req.data[1] = mac;
    req.data[2] = ip;
    return req;
}

QueryRequest MakeCountPlayerByMacIP(const string& mac, const string& ip, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(2);
    req.data[0] = mac;
    req.data[1] = ip;
    return req;
}

QueryRequest MakePlayerGrowIDExists(const string& growID, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(1);
    req.data[0] = growID;
    return req;
}

QueryRequest MakePlayerGrowIDCreate(uint32 userID, const string& name, const string& pass, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(3);
    req.data[0] = name;
    req.data[1] = pass;
    req.data[2] = userID;
    return req;
}

QueryRequest MakeGetPlayerByNameAndPass(const string& name, const string& pass, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(2);
    req.data[0] = name;
    req.data[1] = pass;
    return req;
}

QueryRequest MakePlayerGrowIDRename(uint32 userID, const string& name, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(2);
    req.data[0] = name;
    req.data[1] = userID;
    return req;
}

void ExecUpdatePlayerIdentifier(DatabasePool* pPool, uint32 userID, const string& mac, const string& vid, const string& sid, const string& rid, const string& gid, int32 hash, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    string query = "UPDATE Players SET ";

    if(!mac.empty() && mac != "02:00:00:00:00:00") {
        query += "Mac = ?, ";
        req.data.push_back(mac);
    }

    if(!vid.empty()) {
        query += "VID = UNHEX(MD5(?)), ";
        req.data.push_back(vid);
    }

    if(!sid.empty()) {
        query += "SID = UNHEX(MD5(?)), ";
        req.data.push_back(sid);
    }

    if(!rid.empty()) {
        query += "RID = UNHEX(?), ";
        req.data.push_back(rid);
    }

    if(!gid.empty()) {
        query += "GID = UNHEX(MD5(?)), ";
        req.data.push_back(gid);
    }

    query += "Hash = ? WHERE ID = ?;";
    req.data.push_back(hash);
    req.data.push_back(userID);

    DatabaseExec(pPool, query, req, QUERY_FLAG_NONE);
}
