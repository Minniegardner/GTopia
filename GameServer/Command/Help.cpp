#include "Help.h"
#include "Utils/DialogBuilder.h"
#include "Utils/StringUtils.h"
#include "Item/ItemUtils.h"
#include "../Server/GameServer.h"

#include <algorithm>

const CommandInfo& Help::GetInfo()
{
    static CommandInfo info =
    {
        "/help",
        "Show available commands",
        ROLE_PERM_NONE,
        {
            CompileTimeHashString("help"),
            CompileTimeHashString("?")
        }
    };

    return info;
}

void Help::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    const auto& commandInfos = GetGameServer()->GetCommandInfos();
    std::vector<string> commandNames;
    commandNames.reserve(commandInfos.size());

    for(const CommandInfo* pInfo : commandInfos) {
        if(!pInfo || pInfo->aliases.empty()) {
            continue;
        }

        if(!GetGameServer()->CanAccessCommand(pPlayer, *pInfo)) {
            continue;
        }

        commandNames.push_back(pInfo->aliases.front());
    }

    std::sort(commandNames.begin(), commandNames.end(), [](const string& lhs, const string& rhs) {
        return ToLower(lhs) < ToLower(rhs);
    });

    string commands = "`o>> Commands: ";
    for(size_t i = 0; i < commandNames.size(); ++i) {
        if(i == 0) {
            commands += commandNames[i];
        }
        else {
            commands += ", " + commandNames[i];
        }
    }

    commands += "``";
    pPlayer->SendOnConsoleMessage(commands);
}
