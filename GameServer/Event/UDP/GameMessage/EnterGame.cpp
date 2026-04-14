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
    pPlayer->SendOnConsoleMessage("Welcome back to `#GTopia``!`o It's going to be a `#BLAST``!");

    const uint32 onlineFriends = pPlayer->CountOnlineFriends();
    if(onlineFriends > 0) {
        pPlayer->SendOnConsoleMessage("You have `w" + ToString(onlineFriends) + "`o friend(s) online now.");
    }

    pPlayer->SendOnConsoleMessage("Where would you like to go? (`w" + ToString(GetMasterBroadway()->GetGlobalOnlineCount()) + "`o online)");

    const string dailyEventStatus = GetGameServer()->GetDailyEventStatusLine();
    if(!dailyEventStatus.empty()) {
        pPlayer->SendOnConsoleMessage(dailyEventStatus);
    }

    if(pPlayer->CanClaimDailyReward(epochDay)) {
        pPlayer->SendOnConsoleMessage("`3You have a daily reward waiting! Claim it for continued bonuses and a growing streak!``");
    }

    pPlayer->SendOnConsoleMessage("Use `/news` to check the latest server updates.");

    pPlayer->SendOnRequestWorldSelectMenu("");
}