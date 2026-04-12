#include "EnterGame.h"
#include "Server/MasterBroadway.h"

void EnterGame::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_ENTERING_GAME)) {
        return;
    }
    
    pPlayer->RemoveState(PLAYER_STATE_ENTERING_GAME);
    pPlayer->SetState(PLAYER_STATE_IN_GAME);

    pPlayer->SendInventoryPacket();
    pPlayer->SendOnSetBux();
    pPlayer->SendOnConsoleMessage("Welcome back, " + pPlayer->GetRawName() + "!");
    pPlayer->SendOnConsoleMessage("Where would you like to go ? (" + ToString(GetMasterBroadway()->GetGlobalOnlineCount()) + " online)");
    pPlayer->SendOnRequestWorldSelectMenu("");
}