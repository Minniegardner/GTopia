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

QueryRequest MakePlayerByMacReq(const string& mac, const string& rid, uint8 platformType, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(3);
    req.data[0] = mac;
    req.data[1] = rid;
    req.data[2] = platformType;
    return req;
}

QueryRequest MakePlayerCreateReq(const string& guestName, uint8 platformType, uint16 guestID, const string& mac, const string& rid, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(5);
    req.data[0] = guestName;
    req.data[1] = platformType;
    req.data[2] = guestID;
    req.data[3] = mac;
    req.data[4] = rid;
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

QueryRequest MakeSavePlayerReq(uint32 userID, uint32 roleID, const string& inventoryData, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(3);
    req.data[0] = roleID,
    req.data[1] = inventoryData,
    req.data[2] = userID;
    return req;
}