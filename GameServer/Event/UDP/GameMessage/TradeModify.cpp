#include "TradeModify.h"

#include "Item/ItemInfoManager.h"
#include "Utils/StringUtils.h"

namespace {

uint16 ResolveTradeServerItemID(uint16 rawItemID)
{
    const std::vector<ItemInfo>& items = GetItemInfoManager()->GetItems();
    for(const ItemInfo& item : items) {
        if(GetItemInfoManager()->GetClientIDByItemID(item.id) == rawItemID) {
            return item.id;
        }
    }

    return rawItemID;
}

}

void TradeModify::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    auto pItemID = packet.Find(CompileTimeHashString("itemID"));
    if(!pItemID) {
        pItemID = packet.Find(CompileTimeHashString("ItemID"));
    }

    if(!pItemID) {
        return;
    }

    uint32 itemID = 0;
    if(ToUInt(string(pItemID->value, pItemID->size), itemID) != TO_INT_SUCCESS || itemID > UINT16_MAX) {
        return;
    }

    pPlayer->ModifyOffer(ResolveTradeServerItemID((uint16)itemID), 0);
}
