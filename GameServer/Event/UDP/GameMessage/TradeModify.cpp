#include "TradeModify.h"

#include "Utils/StringUtils.h"

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

    uint32 amount = 0;
    auto pAmount = packet.Find(CompileTimeHashString("amount"));
    if(!pAmount) {
        pAmount = packet.Find(CompileTimeHashString("Amount"));
    }
    if(!pAmount) {
        pAmount = packet.Find(CompileTimeHashString("count"));
    }

    if(pAmount) {
        if(ToUInt(string(pAmount->value, pAmount->size), amount) != TO_INT_SUCCESS) {
            amount = 0;
        }
    }

    pPlayer->ModifyOffer((uint16)itemID, (uint16)amount);
}
