#include "Drop.h"
#include "../../../Player/Dialog/DropDialog.h"

void Drop::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    auto pItemID = packet.Find(CompileTimeHashString("itemID"));
    if(!pItemID) {
        return;
    }

    uint32 itemID = 0;
    if(ToUInt(string(pItemID->value, pItemID->size), itemID) != TO_INT_SUCCESS) {
        return;
    }

    if(itemID > UINT16_MAX) {
        return;
    }

    DropDialog::Request(pPlayer, (uint16)itemID);
}