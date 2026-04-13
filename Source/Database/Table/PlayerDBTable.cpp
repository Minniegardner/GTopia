#include "PlayerDBTable.h"

void DatabasePlayerExec(DatabasePool* pPool, ePlayerDBQuery queryID, QueryRequest& req, bool preapred)
{
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

QueryRequest MakeSavePlayerReq(uint32 userID, uint32 roleID, const string& inventoryData, int32 gems, uint32 level, uint32 xp, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(6);
    req.data[0] = roleID,
    req.data[1] = inventoryData,
    req.data[2] = gems,
    req.data[3] = level,
    req.data[4] = xp,
    req.data[5] = userID;
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

void ExecUpdatePlayerIdentifier(DatabasePool* pPool, uint32 userID, const string& mac, const string& vid, const string& rid, const string& gid, int32 hash, int32 ownerID)
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
