#include "EnterGame.h"
#include "Server/MasterBroadway.h"
#include "Server/GameServer.h"
#include "Utils/Timer.h"

void EnterGame::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_ENTERING_GAME)) {
        return;
    }
    
    pPlayer->RemoveState(PLAYER_STATE_ENTERING_GAME);
    pPlayer->SetState(PLAYER_STATE_IN_GAME);

    const uint32 epochDay = (uint32)(Time::GetTimeSinceEpoch() / 86400ull);
    pPlayer->ResetDailyRewardProgressIfNewDay(epochDay);
    pPlayer->SendSetHasGrowID(pPlayer->HasGrowID() ? true : false);

    pPlayer->SendInventoryPacket();
    pPlayer->SendOnSetBux();
    pPlayer->SendOnConsoleMessage("Welcome back, " + pPlayer->GetRawName() + "!");
    pPlayer->SendOnConsoleMessage("Where would you like to go ? (" + ToString(GetMasterBroadway()->GetGlobalOnlineCount()) + " online)");

    const string dailyEventStatus = GetGameServer()->GetDailyEventStatusLine();
    if(!dailyEventStatus.empty()) {
        pPlayer->SendOnConsoleMessage(dailyEventStatus);
    }

    if(pPlayer->CanClaimDailyReward(epochDay)) {
        pPlayer->SendOnConsoleMessage("`3You have a daily reward waiting! Claim it for continued bonuses and a growing streak!``");
    }

    pPlayer->SendOnRequestWorldSelectMenu("");
}