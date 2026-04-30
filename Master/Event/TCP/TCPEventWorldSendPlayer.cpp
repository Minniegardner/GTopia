#include "TCPEventWorldSendPlayer.h"
#include "../../World/WorldManager.h"
#include "../../Server/ServerManager.h"

void TCPEventWorldSendPlayer::Execute(NetClient* pClient, VariantVector& data)
{
    if(!pClient) {
        return;
    }

    ServerInfo* pServer = (ServerInfo*)pClient->data;
    if(!pServer) {
        return;
    }

    GetWorldManager()->HandlePlayerJoinRequest(pServer, std::move(data));
}
