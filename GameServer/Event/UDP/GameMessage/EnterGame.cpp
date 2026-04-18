#include "EnterGame.h"
#include "Server/MasterBroadway.h"
#include "Server/GameServer.h"
#include "World/WorldManager.h"
#include "Utils/StringUtils.h"
#include "Utils/Timer.h"

void EnterGame::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_ENTERING_GAME)) {
        return;
    }

    if(GetGameServer()->IsMaintenance()) {
        Role* pRole = pPlayer->GetRole();
        if(!pRole || !pRole->HasPerm(ROLE_PERM_MAINTENANCE_EXCEPTION)) {
            pPlayer->RemoveState(PLAYER_STATE_ENTERING_GAME);
            pPlayer->SendLogonFailWithLog(pPlayer->GetRawName() + " is currently under maintenance. Please come back later.");
            return;
        }
    }
    
    pPlayer->RemoveState(PLAYER_STATE_ENTERING_GAME);
    pPlayer->SetState(PLAYER_STATE_IN_GAME);

    const uint32 epochDay = (uint32)(Time::GetTimeSinceEpoch() / 86400ull);
    pPlayer->ResetDailyRewardProgressIfNewDay(epochDay);
    pPlayer->SendSetHasGrowID(pPlayer->HasGrowID() ? true : false);

    pPlayer->SendInventoryPacket();
    pPlayer->SendOnSetBux();

    const uint8 loginMode = pPlayer->GetLoginDetail().loginMode;
    const bool silentSubserverLogin = loginMode == LOGIN_MODE_REDIRECT_SUBSERVER_SILENT;

    if(!silentSubserverLogin) {
        pPlayer->SendOnConsoleMessage("`oWelcome back to `5GTopia`o, " + pPlayer->GetRawName() + ".");

        const uint32 onlineFriends = pPlayer->CountOnlineFriends();
        if(onlineFriends > 0) {
            pPlayer->SendOnConsoleMessage("`oYou currently have `9" + ToString(onlineFriends) + "`o friends online.");
        }
        else {
            pPlayer->SendOnConsoleMessage("`oYou currently have no friends online.");
        }

        pPlayer->SendOnConsoleMessage("Where would you like to go? (`w" + ToString(GetMasterBroadway()->GetGlobalOnlineCount()) + " ``online)");

        const string dailyEventStatus = GetGameServer()->GetDailyEventStatusLine();
        if(!dailyEventStatus.empty()) {
            pPlayer->SendOnConsoleMessage(dailyEventStatus);
        }

        if(pPlayer->CanClaimDailyReward(epochDay)) {
            pPlayer->SendOnConsoleMessage("`3You have a daily reward waiting! Claim it for continued bonuses and a growing streak!``");
        }

        pPlayer->SendOnConsoleMessage("Use `/news` to check the latest server updates.");
        pPlayer->NotifyFriendsStatusChange(true);
    }

    if((loginMode == LOGIN_MODE_REDIRECT_SUBSERVER || loginMode == LOGIN_MODE_REDIRECT_SUBSERVER_SILENT) && !pPlayer->GetLoginDetail().doorID.empty()) {
        string redirectTarget = ToUpper(pPlayer->GetLoginDetail().doorID);
        RemoveAllSpaces(redirectTarget);

        string doorID;
        auto splitPos = redirectTarget.find(':');
        if(splitPos != string::npos) {
            doorID = redirectTarget.substr(splitPos + 1);
            redirectTarget = redirectTarget.substr(0, splitPos);
        }

        if(!redirectTarget.empty()) {
            pPlayer->SetPendingDoorWarpID(doorID);
            GetWorldManager()->PlayerJoinRequest(pPlayer, redirectTarget);
            return;
        }
    }

    pPlayer->SendOnRequestWorldSelectMenu("");
}