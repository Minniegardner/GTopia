#include "WorldDBTable.h"

void DatabaseWorldExec(DatabasePool* pPool, eWorldDBQuery queryID, QueryRequest& req, bool preapred)
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

QueryRequest MakeWorldExistsByName(const string& worldName, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;    

    req.data.resize(1);
    req.data[0] = worldName;
    return req;
}

QueryRequest MakeWorldCreate(const string& worldName, uint32 homeServerID, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(2);
    req.data[0] = worldName;
    req.data[1] = homeServerID;
    return req;
}

QueryRequest MakeGetWorldData(const string& worldName, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(1);
    req.data[0] = worldName;
    return req;
}

QueryRequest MakeSaveWorld(const string& worldName, uint32 worldID, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(2);
    req.data[0] = worldName;
    req.data[1] = worldID;
    return req;
}

QueryRequest MakeSetWorldHomeServer(uint32 homeServerID, uint32 worldID, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(2);
    req.data[0] = homeServerID;
    req.data[1] = worldID;
    return req;
}
