#include "Utils/StringUtils.h"
#include "GiveItem.h"
#include "../Server/GameServer.h"
#include "../Server/MasterBroadway.h"
#include "Item/ItemInfoManager.h"

const CommandInfo& GiveItem::GetInfo()
{
    static CommandInfo info =
    {
        "/giveitem <userID> <amount> <item name>",
        "Give item to player",
        ROLE_PERM_COMMAND_GIVEITEM,
        {
            CompileTimeHashString("giveitem")
        }
    };

    return info;
}

void GiveItem::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() < 4) {
        pPlayer->SendOnConsoleMessage("`oUsage: " + GetInfo().usage);
        return;
    }

    uint32 userID = 0;
    if(ToUInt(args[1], userID) != TO_INT_SUCCESS) {
        pPlayer->SendOnConsoleMessage("`oUserID must be number!");
        return;
    }

    uint32 amount = 0;
    if(ToUInt(args[2], amount) != TO_INT_SUCCESS) {
        pPlayer->SendOnConsoleMessage("`oItem amount must be number!");
        return;
    }

    string itemName = JoinString(args, " ", 3);
    ItemInfo* pItem = GetItemInfoManager()->GetItemByName(itemName);
    if(!pItem) {
        pPlayer->SendOnConsoleMessage("`oFailed to find given item " + itemName);
        return;
    }

    GamePlayer* pTarget = GetGameServer()->GetPlayerByUserID(userID);
    if(!pTarget) {
        GetMasterBroadway()->SendCrossServerActionRequest(
            TCP_CROSS_ACTION_GIVEITEM,
            pPlayer->GetUserID(),
            pPlayer->GetRawName(),
            ToString(userID),
            true,
            ToString(pItem->id),
            amount);
        pPlayer->SendOnConsoleMessage("`oGiveitem request sent across subserver for user ID `w" + ToString(userID) + "``.");
        return;
    }

    uint8 givenCount = pTarget->GetInventory().AddItem(pItem->id, amount, pTarget);
    if(givenCount == 0) {
        /**
         * handle
         */
    }

    pPlayer->SendOnConsoleMessage("`oGiven " + ToString(givenCount) + "x " + pItem->name + " to " + pTarget->GetRawName() + " (ID: " + ToString(pTarget->GetUserID()) + ")");
    pTarget->SendOnConsoleMessage("`oGiven: " + ToString(givenCount) + "x " + pItem->name);
}
