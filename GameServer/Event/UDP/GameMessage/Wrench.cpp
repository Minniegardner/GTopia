#include "Wrench.h"

#include "../../../Server/GameServer.h"
#include "Item/ItemInfoManager.h"
#include "Utils/StringUtils.h"

void Wrench::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    pPlayer->CancelTrade(true);

    auto pNetIDField = packet.Find(CompileTimeHashString("netid"));
    if(!pNetIDField) {
        pNetIDField = packet.Find(CompileTimeHashString("netID"));
    }

    if(!pNetIDField) {
        return;
    }

    int32 targetNetID = 0;
    if(ToInt(string(pNetIDField->value, pNetIDField->size), targetNetID) != TO_INT_SUCCESS) {
        return;
    }

    GamePlayer* pTarget = GetGameServer()->GetPlayerByNetID(targetNetID);
    if(!pTarget) {
        return;
    }

    if(pTarget->GetCurrentWorld() == 0 || pTarget->GetCurrentWorld() != pPlayer->GetCurrentWorld()) {
        return;
    }

    if(pTarget->GetUserID() == pPlayer->GetUserID()) {
        pPlayer->SendWrenchSelf("PlayerInfo");
    }
    else {
        pPlayer->SendWrenchOthers(pTarget);
    }
}