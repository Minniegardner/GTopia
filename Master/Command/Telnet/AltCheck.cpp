#include "AltCheck.h"
#include "../../Context.h"
#include "../../Player/PlayerManager.h"
#include "../../../Source/Database/Table/PlayerDBTable.h"

void AltCheckDBCB(QueryTaskResult&& result)
{
    TelnetClient* pNetClient = GetTelnetServer()->GetClientByNetID(result.ownerID);
    if(!pNetClient) return;

    if(!result.result) {
        pNetClient->SendMessage("DB query failed.", true);
        pNetClient->SetBusy(false);
        return;
    }

    int cnt = result.result->GetRowCount();
    pNetClient->SendMessage(string("DB matches: ") + ToString(cnt), true);
    pNetClient->SetBusy(false);
}

const TelnetCommandInfo& AltCheck::GetInfo()
{
    static TelnetCommandInfo info =
    {
        "/altcheck <query>",
        "Check for potential alternate accounts by name or userID",
        2,
        {
            CompileTimeHashString("altcheck")
        }
    };

    return info;
}

void AltCheck::Execute(TelnetClient* pNetClient, std::vector<string>& args)
{
    if(!pNetClient || !TelnetCommandBase<AltCheck>::CheckPerm(pNetClient)) {
        return;
    }

    if(args.size() < 2) {
        if(pNetClient) TelnetCommandBase<AltCheck>::SendUsage(pNetClient);
        return;
    }

    string query = args[1];

    // If numeric, treat as userID and inspect live sessions + DB counts by IP
    uint32 userID = 0;
    if(ToUInt(query, userID) == TO_INT_SUCCESS && userID > 0) {
        PlayerSession* pSession = GetPlayerManager()->GetSessionByID(userID);
        if(!pSession) {
            pNetClient->SendMessage("Session not found for userID.", true);
            return;
        }

        string ip = pSession->ip;
        pNetClient->SendMessage(string("Live session IP: ") + ip, true);

        // list live sessions sharing the same IP
        auto all = GetPlayerManager()->FindPlayerSessionsByNamePrefix("", false);
        for(auto s : all) {
            if(!s) continue;
            if(s->ip == ip) {
                pNetClient->SendMessage(string("-> ") + ToString(s->userID) + " - " + s->name, true);
            }
        }

        // query DB for same IP
        pNetClient->SetBusy(true);
        QueryRequest req = PlayerDB::CountByIP(ip, pNetClient->GetNetID());
        req.callback = &AltCheckDBCB;
        DatabasePlayerExec(GetContext()->GetDatabasePool(), req);
        return;
    }

    // otherwise search live sessions by name prefix
    auto results = GetPlayerManager()->FindPlayerSessionsByNamePrefix(query, false);
    if(results.empty()) {
        pNetClient->SendMessage("No live sessions found for query.", true);
        return;
    }

    pNetClient->SendMessage("Live matches:", true);
    for(auto s : results) {
        if(!s) continue;
        pNetClient->SendMessage(string("-> ") + ToString(s->userID) + " - " + s->name + " - " + s->ip, true);
    }
}
