#include "EnterGame.h"

void EnterGame::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer) {
        return;
    }
    
    pPlayer->RemoveState(PLAYER_STATE_ENTERING_GAME);
    pPlayer->SetState(PLAYER_STATE_IN_GAME);

    pPlayer->SendInventoryPacket();
    pPlayer->SendOnRequestWorldSelectMenu("");
}