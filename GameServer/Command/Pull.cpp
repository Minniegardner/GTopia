#include "Pull.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"

const CommandInfo& Pull::GetInfo()
{
    static CommandInfo info =
    {
        "/pull <player_prefix>",
        "Pull a player to your position",
        ROLE_PERM_COMMAND_PULL,
        {
            CompileTimeHashString("pull")
        }
    };

    return info;
}

void Pull::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() < 2) {
        pPlayer->SendOnConsoleMessage("`oUsage: " + GetInfo().usage);
        return;
    }

    if(pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    const string query = args[1];
    if(query.size() < 3) {
        pPlayer->SendOnConsoleMessage("You'll need to enter at least the first three characters of the person's name.");
        return;
    }

    auto matches = GetGameServer()->FindPlayersByNamePrefix(query, true, pPlayer->GetCurrentWorld());
    if(matches.empty()) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` There is nobody currently in this server with a name starting with `w" + query + "``.");
        return;
    }

    std::vector<GamePlayer*> filtered;
    filtered.reserve(matches.size());
    for(GamePlayer* pMatch : matches) {
        if(pMatch && pMatch != pPlayer) {
            filtered.push_back(pMatch);
        }
    }

    if(filtered.empty()) {
        pPlayer->SendOnConsoleMessage("`4Oops:`` There is nobody currently in this server with a name starting with `w" + query + "``.");
        return;
    }

    if(filtered.size() > 1) {
        pPlayer->SendOnConsoleMessage("`oThere are more than two players starting with `w" + query + "`o, be more specific.");
        return;
    }

    GamePlayer* pTarget = filtered[0];

    const Vector2Float srcPos = pPlayer->GetWorldPos();
    const float pullX = std::max(0.0f, srcPos.x + 32.0f);
    const float pullY = std::max(0.0f, srcPos.y);

    pTarget->SetWorldPos(pullX, pullY);
    pTarget->SetRespawnPos(pullX, pullY);
    pTarget->SendPositionToWorldPlayers();

    pTarget->SendOnConsoleMessage("`oPulled by ``" + pPlayer->GetDisplayName() + "``.");
    pPlayer->SendOnConsoleMessage("`oPulled ``" + pTarget->GetDisplayName() + "``.");
}
