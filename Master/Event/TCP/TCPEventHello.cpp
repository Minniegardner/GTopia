#include "TCPEventHello.h"
#include "../../Server/ServerManager.h"
#include "Utils/StringUtils.h"

#include "IO/Log.h"

void TCPEventHello::Execute(NetClient* pClient, VariantVector& data)
{
    pClient->data = new NetServerInfo();

    uint8 bytes[16];
    if(GetRandomBytes(&bytes, sizeof(bytes)) < 0) {
        pClient->status = SOCKET_CLIENT_CLOSE;
        return;
    }

    string authKey = ToHex(bytes, sizeof(bytes));
    ((NetServerInfo*)pClient->data)->authKey = authKey;

    GetServerManager()->SendHelloPacket(authKey, pClient->connectionID);
}