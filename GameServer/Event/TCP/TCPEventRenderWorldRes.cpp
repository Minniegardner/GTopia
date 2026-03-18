#include "TCPEventRenderWorldRes.h"
#include "../../Server/GameServer.h"
#include "../../Player/GamePlayer.h"
#include "IO/Log.h"

void TCPEventRenderWorldRes::Execute(NetClient* pClient, VariantVector& data)
{
    GamePlayer* pPlayer = GetGameServer()->GetPlayerByUserID(data[2].GetUINT());
    if(!pPlayer) {
        LOGGER_LOG_WARN("Received player session packet but player not found?");
        return;
    }

    pPlayer->OnHandleTCP(std::move(data));
}