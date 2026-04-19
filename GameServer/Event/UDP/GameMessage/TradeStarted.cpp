#include "TradeStarted.h"

#include "../../../Server/GameServer.h"
#include "../../../World/WorldManager.h"
#include "Utils/StringUtils.h"

void TradeStarted::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    World* pWorld = GetWorldManager()->GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

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

    GamePlayer* pTarget = GetGameServer()->GetPlayerByNetID((uint32)targetNetID);
    if(!pTarget || pTarget == pPlayer || !pTarget->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    if(pTarget->GetCurrentWorld() != pPlayer->GetCurrentWorld()) {
        return;
    }

    pPlayer->StartTrade(pTarget);
}
