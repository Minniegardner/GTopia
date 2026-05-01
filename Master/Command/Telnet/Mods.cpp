#include "Mods.h"
#include "../../Context.h"
#include "../../Player/PlayerManager.h"

const TelnetCommandInfo& Mods::GetInfo()
{
    static TelnetCommandInfo info =
    {
        "/mods",
        "List moderator accounts currently connected (best-effort)",
        2,
        {
            CompileTimeHashString("mods")
        }
    };

    return info;
}

void Mods::Execute(TelnetClient* pNetClient, std::vector<string>& args)
{
    if(!pNetClient || !TelnetCommandBase<Mods>::CheckPerm(pNetClient)) return;

    auto all = GetPlayerManager()->FindPlayerSessionsByNamePrefix("", false);
    pNetClient->SendMessage("Connected sessions (use /altcheck <name> for details):", true);
    int printed = 0;
    for(auto s : all) {
        if(!s) continue;
        pNetClient->SendMessage(string("-> ") + ToString(s->userID) + " - " + s->name + " - " + s->ip, true);
        if(++printed >= 200) break;
    }

    if(printed == 0) pNetClient->SendMessage("No connected sessions.", true);
}
