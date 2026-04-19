#include "AddLevel.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"

const CommandInfo& AddLevel::GetInfo()
{
    static CommandInfo info =
    {
        "/addlevel <name> <amount>",
        "Add or reduce player's level",
        ROLE_PERM_COMMAND_GIVEITEM,
        {
            CompileTimeHashString("addlevel")
        }
    };

    return info;
}

void AddLevel::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() != 3) {
        pPlayer->SendOnConsoleMessage("Command incomplete. Here's the argument list:");
        pPlayer->SendOnConsoleMessage("-> /addlevel <name> <amount>");
        return;
    }

    int32 amount = 0;
    if(ToInt(args[2], amount) != TO_INT_SUCCESS) {
        pPlayer->SendOnConsoleMessage("`4Amount must be a number.");
        return;
    }

    string query = ToLower(args[1]);
    auto players = GetGameServer()->FindPlayersByNamePrefix(query, false, 0);
    if(players.empty()) {
        pPlayer->SendOnConsoleMessage("Player '" + args[1] + "' not found or offline");
        return;
    }

    if(players.size() > 1) {
        pPlayer->SendOnConsoleMessage("`oThere are multiple players starting with `w" + query + "`o, be more specific.");
        return;
    }

    GamePlayer* pTarget = players[0];
    if(!pTarget) {
        pPlayer->SendOnConsoleMessage("Player '" + args[1] + "' not found or offline");
        return;
    }

    const int32 currentLevel = static_cast<int32>(pTarget->GetLevel());
    int32 nextLevel = currentLevel + amount;
    if(nextLevel < 1) {
        nextLevel = 1;
    }

    pTarget->SetLevel(static_cast<uint32>(nextLevel));

    pPlayer->SendOnConsoleMessage("`oSet `$" + pTarget->GetRawName() + "`o's level to `$" + ToString(pTarget->GetLevel()) + "``");
    pTarget->SendOnConsoleMessage("`oYour level is now `$" + ToString(pTarget->GetLevel()) + "``");
}
