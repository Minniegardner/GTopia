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

    std::vector<const CommandInfo*> availableCommands;
    availableCommands.reserve(commandInfos.size());

    for(const CommandInfo* pInfo : commandInfos) {
        if(!pInfo || pInfo->usage.empty()) {
            continue;
        }

        if(!GetGameServer()->CanAccessCommand(pPlayer, *pInfo)) {
            continue;
        }

        availableCommands.push_back(pInfo);
    }

    std::sort(availableCommands.begin(), availableCommands.end(), [](const CommandInfo* lhs, const CommandInfo* rhs) {
        return ToLower(lhs->usage) < ToLower(rhs->usage);
    });

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wHelp``", ITEM_ID_WRENCH, true)
        ->AddTextBox("`oRole: `w" + pPlayer->GetRole()->GetName() + "``")
        ->AddSpacer();

    if(availableCommands.empty()) {
        db.AddTextBox("`4No command available for your role.");
    }
    else {
        for(const CommandInfo* pInfo : availableCommands) {
            db.AddTextBox("`w" + pInfo->usage + "`` - " + pInfo->desc);
        }
    }

    db.EndDialog("command_help", "", "Close");
    pPlayer->SendOnDialogRequest(db.Get());
}
