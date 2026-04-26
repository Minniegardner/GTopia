#include "TCPEventAuth.h"
#include "../../Server/ServerManager.h"
#include "Utils/StringUtils.h"
#include "Utils/Timer.h"
#include "IO/Log.h"

void TCPAuthEventData::FromVariant(const VariantVector& varVec)
{
    if(varVec.size() < 4) {
        return;
    }

    authKey = varVec[1].GetString();
    serverID = varVec[2].GetUINT();
    serverType = varVec[3].GetINT();
}

void TCPEventAuth::Execute(NetClient* pClient, VariantVector& data)
{
    TCPAuthEventData eventData;
    eventData.FromVariant(data);

    ServerInfo* pServerInfo = (ServerInfo*)pClient->data;
    if(!pServerInfo) {
        return;
    }

    if(pServerInfo->authKey != eventData.authKey) {
        LOGGER_LOG_WARN("Failed to authorize server! closing connection...");
        pClient->status = SOCKET_CLIENT_CLOSE;
        return;
    }

    GetServerManager()->AddServer(pServerInfo, eventData.serverID, eventData.serverType);
    GetServerManager()->SendAuthPacket(pServerInfo, true);
}