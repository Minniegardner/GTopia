#include "Utils/StringUtils.h"
#include "GiveItem.h"
#include "CommandTargetUtils.h"
#include "../Server/GameServer.h"
#include "../Server/MasterBroadway.h"
#include "Item/ItemInfoManager.h"

const CommandInfo& GiveItem::GetInfo()
{
    static CommandInfo info =
    {
        "/giveitem <target|#userid> <amount> <item name>",
        "Give item to player",
        ROLE_PERM_COMMAND_GIVEITEM,
        {
            CompileTimeHashString("giveitem"),
            CompileTimeHashString("get")
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
        pPlayer->SendOnConsoleMessage("Usage: " + GetInfo().usage);
        return;
    }

    CommandTargetSpec targetSpec;
    string targetError;
    if(!ParseCommandTargetArg(args[1], true, targetSpec, targetError)) {
        pPlayer->SendOnConsoleMessage(targetError);
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

    auto matches = ResolveLocalTargets(targetSpec, false, 0);
    if(!targetSpec.exactMatch && !targetSpec.byUserID && matches.size() > 1) {
        SendAmbiguousLocalTargetList(pPlayer, targetSpec.query, matches, "server");
        return;
    }

    GamePlayer* pTarget = matches.empty() ? nullptr : matches[0];
    if(!pTarget) {
        GetMasterBroadway()->SendCrossServerActionRequest(
            TCP_CROSS_ACTION_GIVEITEM,
            pPlayer->GetUserID(),
            pPlayer->GetRawName(),
            targetSpec.query,
            targetSpec.exactMatch,
            ToString(pItem->id),
            amount);
        pPlayer->SendOnConsoleMessage("`oGiveitem request sent across subserver for target `w" + targetSpec.query + "``.");
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
