#include "ClearInv.h"
#include "Item/ItemUtils.h"

const CommandInfo& ClearInv::GetInfo()
{
    static CommandInfo info =
    {
        "/clearinv",
        "Clear your inventory",
        ROLE_PERM_COMMAND_GIVEITEM,
        {
            CompileTimeHashString("clearinv")
        }
    };

    return info;
}

void ClearInv::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    std::vector<std::pair<uint16, uint8>> items;
    const auto& invItems = pPlayer->GetInventory().GetItems();
    items.reserve(invItems.size());

    for(const auto& item : invItems) {
        if(item.id == ITEM_ID_FIST || item.id == ITEM_ID_WRENCH) {
            continue;
        }

        if(item.count == 0) {
            continue;
        }

        items.emplace_back(item.id, item.count);
    }

    for(const auto& [itemID, count] : items) {
        pPlayer->ModifyInventoryItem(itemID, -(int16)count);
    }

    pPlayer->SendOnConsoleMessage("`#** Inventory cleared.");
}
