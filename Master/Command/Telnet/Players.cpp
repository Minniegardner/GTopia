#include "Players.h"
#include "../../Context.h"
#include "../../Player/PlayerManager.h"

const TelnetCommandInfo& Players::GetInfo()
{
    static TelnetCommandInfo info =
    {
        "/players [query]",
        "List connected player sessions (optional name prefix)",
        2,
        {
            CompileTimeHashString("players")
        }
    };

    return info;
}

void Players::Execute(TelnetClient* pNetClient, std::vector<string>& args)
{
    if(!pNetClient || !TelnetCommandBase<Players>::CheckPerm(pNetClient)) return;

    string query = "";
    if(args.size() > 1) query = args[1];

    auto results = GetPlayerManager()->FindPlayerSessionsByNamePrefix(query, false);
    pNetClient->SendMessage("Players:", true);

    int printed = 0;
    for(auto pSession : results) {
        if(!pSession) continue;
        string line = to_string(pSession->userID) + " - " + pSession->name + " - " + pSession->ip + " - " + pSession->worldName;
        pNetClient->SendMessage(line, true);
        if(++printed >= 200) break;
    }

    if(printed == 0) pNetClient->SendMessage("No players found.", true);
}
