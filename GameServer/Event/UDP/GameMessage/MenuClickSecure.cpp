#include "MenuClickSecure.h"

#include "../../../Player/Dialog/DropDialog.h"
#include "Utils/StringUtils.h"

void MenuClickSecure::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    auto pItem = packet.Find(CompileTimeHashString("itemID"));
    if(!pItem) {
        return;
    }

    uint32 itemID = 0;
    if(ToUInt(string(pItem->value, pItem->size), itemID) != TO_INT_SUCCESS) {
        return;
    }

    DropDialog::Request(pPlayer, (uint16)itemID);
}