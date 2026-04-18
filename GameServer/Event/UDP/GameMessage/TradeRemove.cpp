#include "TradeRemove.h"

#include "Utils/StringUtils.h"

void TradeRemove::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
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

    pPlayer->RemoveOffer((uint16)itemID);
}
