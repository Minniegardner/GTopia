#include "TCPEventHeartBeat.h"
#include "../../Server/ServerManager.h"
#include "Packet/GamePacket.h"

void TCPHeartBeatEventData::FromVariant(VariantVector& varVec)
{
    if(varVec.size() < 3) {
        return;
    }

    playerCount = varVec[1].GetUINT();
    worldCount = varVec[2].GetUINT();
}

void TCPEventHeartBeat::Execute(NetClient* pClient, VariantVector& data)
{
    if(!pClient) {
        return;
    }

    ServerInfo* pServer = (ServerInfo*)pClient->data;
    if(!pServer) {
        return;
    }

    TCPHeartBeatEventData eventData;
    eventData.FromVariant(data);

    pServer->playerCount = eventData.playerCount;
    pServer->worldCount = eventData.worldCount;
    pServer->lastHeartbeatTime.Reset();
}
