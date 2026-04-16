#include "TCPEventAuth.h"
#include "IO/Log.h"
#include "Server/MasterBroadway.h"

void TCPEventAuth::Execute(NetClient* pClient, VariantVector& data)
{
    LOGGER_LOG_INFO("Server is authed by master");
    GetMasterBroadway()->SendHeartBeat();
}