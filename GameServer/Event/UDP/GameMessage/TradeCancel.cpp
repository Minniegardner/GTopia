#include "TradeCancel.h"

void TradeCancel::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    pPlayer->CancelTrade(false);
}
