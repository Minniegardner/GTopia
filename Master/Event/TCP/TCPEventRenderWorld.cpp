#include "TCPEventRenderWorld.h"
#include "../../Server/ServerManager.h"

#include "IO/Log.h"

void TCPEventRenderWorld::Execute(NetClient* pClient, VariantVector& data)
{
    RendererInfo* pRenderServer = GetServerManager()->GetBestRenderer();
    if(!pRenderServer) {
        VariantVector packet(4);
        packet[0] = TCP_PACKET_RENDER_WORLD_RES;
        packet[1] = TCP_RENDER_WORLD_FAIL;
        packet[2] = data[2].GetUINT();
        packet[3] = data[1].GetUINT();

        pClient->Send(packet);
        return;
    }

    VariantVector packet(3);
    packet[0] = TCP_PACKET_RENDER_WORLD;
    packet[1] = data[1].GetUINT();
    packet[2] = data[2].GetUINT();

    GetServerManager()->SendPacketRendererRaw(pRenderServer->serverID, packet);
}