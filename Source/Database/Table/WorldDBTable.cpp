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

QueryRequest MakeWorldCreate(const string& worldName, int32 ownerID)
{
    QueryRequest req;
    req.ownerID = ownerID;

    req.data.resize(1);
    req.data[0] = worldName;
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
