#include "WarpTo.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"
#include "../World/WorldManager.h"

const CommandInfo& WarpTo::GetInfo()
{
    static CommandInfo info =
    {
        "/warpto <player_prefix>",
        "Warp to a player's current world",
        ROLE_PERM_COMMAND_WARPTO,
        {
            CompileTimeHashString("warpto")
        }
    };

    return info;
}

void WarpTo::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() < 2 || args[1].size() < 3) {
        pPlayer->SendOnConsoleMessage("`6>> `4Oops: `6Enter at least the `#first three characters `6of the persons name.``");
        return;
    }

    string query = args[1];
    bool exactMatch = false;
    if(!query.empty() && query[0] == '/') {
        exactMatch = true;
        query.erase(query.begin());
    }

    auto matches = GetGameServer()->FindPlayersByNamePrefix(query, false, 0);
    if(matches.empty()) {
        pPlayer->SendOnConsoleMessage("`6>> No one online who has a name starting with `$" + query + "`6.``");
        return;
    }

    if(!exactMatch && matches.size() > 1) {
        pPlayer->SendOnConsoleMessage("`oThere are more than two players in the server starting with `w" + query + " `obe more specific!");
        return;
    }

    GamePlayer* pTarget = matches[0];
    if(!pTarget || pTarget == pPlayer) {
        pPlayer->SendOnConsoleMessage("Nope, you can't use that on yourself.");
        return;
    }

    const uint32 targetWorldID = pTarget->GetCurrentWorld();
    if(targetWorldID == 0) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` Target player is not in a world right now.");
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(targetWorldID);
    if(!pWorld) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` Unable to locate that world right now.");
        return;
    }

    const string worldName = pWorld->GetWorlName();
    pPlayer->SendOnConsoleMessage("`oWarping to `w" + pTarget->GetRawName() + "`o at `w" + worldName + "``...");
    pPlayer->SendOnTextOverlay("Warping to `w" + pTarget->GetRawName() + "`` at (`2" + worldName + "``)..");

    GetWorldManager()->PlayerJoinRequest(pPlayer, worldName);
}
