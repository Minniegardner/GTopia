#include "InvSee.h"
#include "Utils/StringUtils.h"
#include "Utils/DialogBuilder.h"
#include "../Server/GameServer.h"
#include "Item/ItemInfoManager.h"
#include "Item/ItemUtils.h"

const CommandInfo& InvSee::GetInfo()
{
    static CommandInfo info =
    {
        "/invsee <name>",
        "Show target inventory",
        ROLE_PERM_COMMAND_MOD,
        {
            CompileTimeHashString("invsee")
        }
    };

    return info;
}

void InvSee::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() < 2 || args[1].size() < 3) {
        pPlayer->SendOnConsoleMessage("`6>> `4Oops: `6Enter at least the `#first three characters `6of the persons name.``");
        return;
    }

    string username = args[1];
    bool exactMatch = false;
    if(!username.empty() && username[0] == '/') {
        exactMatch = true;
        username.erase(username.begin());
    }

    if(username.empty()) {
        pPlayer->SendOnConsoleMessage("`6>> `4Oops: `6Enter at least the `#first three characters `6of the persons name.``");
        return;
    }

    const string myNameLower = ToLower(pPlayer->GetRawName());
    const string targetQueryLower = ToLower(username);
    if((exactMatch && myNameLower == targetQueryLower) || (!exactMatch && myNameLower.rfind(targetQueryLower, 0) == 0)) {
        pPlayer->SendOnConsoleMessage("Nope, you can't InvSee yourself.");
        return;
    }

    auto matches = GetGameServer()->FindPlayersByNamePrefix(username, true, pPlayer->GetCurrentWorld());
    if(matches.empty()) {
        pPlayer->SendOnConsoleMessage("`4Oops: ``No Players Found.");
        return;
    }

    if(matches.size() > 1) {
        pPlayer->SendOnConsoleMessage("`oThere are more than two players in the server starting with `w" + username + " `obe more specific!");
        return;
    }

    GamePlayer* pTarget = matches[0];
    if(!pTarget) {
        pPlayer->SendOnConsoleMessage("`4Oops: ``No Players Found.");
        return;
    }

    DialogBuilder db;
    db.SetDefaultColor('o');
    db.AddQuickExit();
    db.AddLabelWithIcon("`w" + pTarget->GetRawName() + "``'s Inventory", ITEM_ID_FIST, true);
    db.AddLabel("Max Size: " + ToString(pTarget->GetInventory().GetCapacity()));
    db.AddLabel("Current Size: " + ToString(pTarget->GetInventory().GetItemTypesCount()));

    for(const auto& invItem : pTarget->GetInventory().GetItems()) {
        ItemInfo* pItem = GetItemInfoManager()->GetItemByID(invItem.id);
        if(!pItem) {
            continue;
        }

        db.AddLabelWithIcon("(`w" + ToString((int32)invItem.count) + "``) " + pItem->name, invItem.id);
    }

    db.EndDialog("InvSee", "", "");
    pPlayer->SendOnDialogRequest(db.Get());
}
