#include "TCPEventHello.h"
#include "../../Server/ServerManager.h"
#include "Utils/StringUtils.h"

void TCPEventHello::Execute(NetClient* pClient, VariantVector& data)
{
    if(!pClient) {
        return;
    }

    ServerInfo* pServer = (ServerInfo*)pClient->data;
    if(!pServer) {
        return;
    }

    uint8 bytes[16];
    if(GetRandomBytes(&bytes, sizeof(bytes)) < 0) {
        pClient->status = SOCKET_CLIENT_CLOSE;
        return;
    }

    string authKey = ToHex(bytes, sizeof(bytes));
    pServer->authKey = authKey;

    GetServerManager()->SendHelloPacket(pServer, authKey);
}