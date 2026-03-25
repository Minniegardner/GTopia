#include "TCPEventKillServer.h"
#include "../../Context.h"

void TCPEventKillServer::Execute(NetClient* pClient, VariantVector& data)
{
    GetContext()->Shutdown();
}
