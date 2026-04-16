#include "ItemInfo.h"
#include "Utils/StringUtils.h"
#include "Item/ItemInfoManager.h"

const CommandInfo& ItemInfoCmd::GetInfo()
{
    static CommandInfo info =
    {
        "/iteminfo <item_name>",
        "Show item details",
        ROLE_PERM_COMMAND_MOD,
        {
            CompileTimeHashString("iteminfo")
        }
    };

    return info;
}

void ItemInfoCmd::Execute(GamePlayer* pPlayer, std::vector<string>& args)
{
    if(!pPlayer || args.empty() || !CheckPerm(pPlayer)) {
        return;
    }

    if(args.size() < 2) {
        pPlayer->SendOnConsoleMessage("Command incomplete. Here's the argument list:");
        pPlayer->SendOnConsoleMessage("-> /iteminfo <item_name>");
        return;
    }

    string sentence;
    for(size_t i = 1; i < args.size(); ++i) {
        sentence += args[i];
    }

    ItemInfo* pItem = GetItemInfoManager()->GetItemByName(sentence);
    if(!pItem) {
        pPlayer->SendOnConsoleMessage("`4Oops: ``Item not found.");
        return;
    }

    pPlayer->SendOnConsoleMessage("Item Info:");
    pPlayer->SendOnConsoleMessage(
        "Name: " + pItem->name +
        ", ID: " + ToString((int32)pItem->id) +
        ", Rarity: " + ToString((int32)pItem->rarity) +
        ", Type: " + ToString((int32)pItem->type) +
        "\nTexture: " + pItem->textureFile +
        " (" + ToString((int32)pItem->textureX) + "," + ToString((int32)pItem->textureY) + ")" +
        "\nExtra File: " + pItem->extraString
    );

    pPlayer->SendOnConsoleMessage(
        "Chi: " + ToString((int32)pItem->material) +
        ", Seed Base: " + ToString((int32)pItem->seedBg) +
        ", Seed Color: " + ToString((int32)pItem->seedFg) +
        ", Seed Overlay Color: " + ToString((int32)pItem->seedBgColor.r) + "," + ToString((int32)pItem->seedBgColor.g) + "," + ToString((int32)pItem->seedBgColor.b) +
        ", Seed Overlay: " + ToString((int32)pItem->seedFgColor.r) + "," + ToString((int32)pItem->seedFgColor.g) + "," + ToString((int32)pItem->seedFgColor.b)
    );
}
