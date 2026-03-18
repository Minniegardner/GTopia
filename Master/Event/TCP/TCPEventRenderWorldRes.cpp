#include "TCPEventRenderWorldRes.h"
#include "../../Server/ServerManager.h"
#include "../../Server/GameServer.h"

void TCPEventRenderWorldRes::Execute(NetClient* pClient, VariantVector& data)
{
    PlayerSession* pPlayer = GetGameServer()->GetPlayerSessionByUserID(data[2].GetUINT());
    if(!pPlayer) {
        LOGGER_LOG_ERROR("Tried to send back render results but player %d not found", data[2].GetUINT());
        return;
    }

    ServerInfo* pServer = GetServerManager()->GetServerByID(pPlayer->serverID);
    if(!pServer) {
        LOGGER_LOG_ERROR("Tried to send back render results but server %d not found", pPlayer->serverID);
        return;
    }

    VariantVector packet(4);
    packet[0] = TCP_PACKET_RENDER_WORLD_RES;
    packet[1] = data[1].GetINT();
    packet[2] = data[2].GetUINT();
    packet[3] = data[3].GetUINT();

    GetServerManager()->SendPacketServerRaw(pServer->serverID, packet);
}