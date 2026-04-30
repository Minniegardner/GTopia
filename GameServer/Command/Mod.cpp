#include "Mod.h"
#include "Utils/DialogBuilder.h"
#include "Item/ItemUtils.h"
#include "Utils/StringUtils.h"
#include "../Server/GameServer.h"

namespace {

void SendModSearchDialog(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wModeration``", ITEM_ID_WRENCH, true)
        ->AddTextBox("`oSearch a player by nickname prefix.")
        ->AddTextInput("target_name", "Player Name", "First 3 letters", 24)
        ->EndDialog("command_mod_query", "Search", "Cancel");

    pPlayer->SendOnDialogRequest(db.Get());
}

void SendModActionsDialog(GamePlayer* pPlayer, const string& query, const std::vector<GamePlayer*>& matches)
{
    if(!pPlayer) {
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o')
        ->AddLabelWithIcon("`wModeration Results``", ITEM_ID_WRENCH, true)
        ->AddTextBox("`oQuery: `w" + query + "``")
        ->AddTextBox("`oPick an action from the buttons below.")
        ->AddTextInput("warn_reason", "Warn reason", "Please follow the rules.", 80)
        ->AddSpacer();

    uint32 shown = 0;
    for(GamePlayer* pTarget : matches) {
        if(!pTarget || pTarget == pPlayer || pTarget->GetCurrentWorld() != pPlayer->GetCurrentWorld()) {
            continue;
        }

        db.AddTextBox("`w" + pTarget->GetDisplayName() + "`` (`o" + pTarget->GetRawName() + "``)");
        db.AddButton("pull_" + ToString(pTarget->GetNetID()), "`#Pull``", "noflags");
        db.AddButton("kick_" + ToString(pTarget->GetNetID()), "`4Kick``", "noflags");
        db.AddButton("ban_" + ToString(pTarget->GetNetID()), "`4World Ban``", "noflags");
        db.AddButton("warn_" + ToString(pTarget->GetNetID()), "Warn", "noflags");
        db.AddButton("warpto_" + ToString(pTarget->GetNetID()), "Warp To", "noflags");
        db.AddSpacer();

        ++shown;
        if(shown >= 8) {
            break;
        }
    }

    if(shown == 0) {
        db.AddTextBox("`4No valid target found in your world.");
    }

    db.EndDialog("command_mod_actions", "", "Close");
    pPlayer->SendOnDialogRequest(db.Get());
}

}

const CommandInfo& Mod::GetInfo()
{
    static CommandInfo info =
    {
        "/mod [player_prefix]",
        "Open moderation actions dialog",
        ROLE_PERM_COMMAND_MOD,
        {
            CompileTimeHashString("mod")
        }
    };

    return info;
}

void Mod::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(pPlayer->GetCurrentWorld() == 0) {
        return;
    }

    if(args.size() < 2) {
        SendModSearchDialog(pPlayer);
        return;
    }

    const string query = args[1];
    if(query.size() < 3) {
        pPlayer->SendOnConsoleMessage("`4Use at least 3 characters from nickname.``");
        return;
    }

    auto matches = GetPlayerManager()->FindPlayersByNamePrefix(query, true, pPlayer->GetCurrentWorld());

    std::vector<GamePlayer*> filtered;
    filtered.reserve(matches.size());
    for(GamePlayer* pTarget : matches) {
        if(pTarget && pTarget != pPlayer) {
            filtered.push_back(pTarget);
        }
    }

    if(filtered.empty()) {
        pPlayer->SendOnConsoleMessage("`4Player not found in your world.``");
        return;
    }

    SendModActionsDialog(pPlayer, query, filtered);
}
