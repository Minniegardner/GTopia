#include "Store.h"
#include "Buy.h"

void Store::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    Buy::SendHome(pPlayer);
}
