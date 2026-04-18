#include "TradeAccept.h"

#include "Utils/StringUtils.h"

void TradeAccept::Execute(GamePlayer* pPlayer, ParsedTextPacket<8>& packet)
{
    if(!pPlayer || !pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    auto pStatus = packet.Find(CompileTimeHashString("status"));
    if(!pStatus) {
        pStatus = packet.Find(CompileTimeHashString("accepted"));
    }

    if(!pStatus) {
        return;
    }

    string statusStr(pStatus->value, pStatus->size);
    int32 status = 0;

    if(statusStr == "true") {
        status = 1;
    }
    else if(statusStr == "false") {
        status = 0;
    }
    else if(ToInt(statusStr, status) != TO_INT_SUCCESS) {
        return;
    }

    pPlayer->AcceptOffer(status > 0);
}
