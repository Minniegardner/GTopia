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

    pPlayer->CancelTrade(false);

    GamePlayer* pTarget = pWorld->GetPlayerByNetID((uint32)targetNetID);
    if(!pTarget || pTarget == pPlayer || !pTarget->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    if(pPlayer->GetTradingWithUserID() != pTarget->GetUserID()) {
        return;
    }

    if(!pTarget->IsTrading()) {
        if(pTarget->GetTradingWithUserID() == pPlayer->GetUserID()) {
            const uint64 nowMS = Time::GetSystemTime();

            pPlayer->SetTrading(true);
            pPlayer->SendTradeStatus(pPlayer);
            pPlayer->SendTradeStatus(pTarget);
            pPlayer->SetLastChangeTradeDeal(nowMS);
            pPlayer->SetTradeAccepted(false);
            pPlayer->SetTradeConfirmed(false);
            pPlayer->SetTradeAcceptedAt(0);
            pPlayer->SetTradeConfirmedAt(0);

            pTarget->SetTrading(true);
            pTarget->SendTradeStatus(pPlayer);
            pTarget->SendTradeStatus(pTarget);
            pTarget->SetLastChangeTradeDeal(nowMS);
            pTarget->SetTradeAccepted(false);
            pTarget->SetTradeConfirmed(false);
            pTarget->SetTradeAcceptedAt(0);
            pTarget->SetTradeConfirmedAt(0);
        }
        else {
            pTarget->SendTradeAlert(pPlayer);
        }
    }
    else {
        pPlayer->SendOnTalkBubble("That player is busy.", true);
        pPlayer->SetTradingWithUserID(0);
    }
}
