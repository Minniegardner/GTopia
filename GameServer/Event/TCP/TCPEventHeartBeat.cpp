#include "TCPEventHeartBeat.h"
#include "Server/MasterBroadway.h"

void TCPEventHeartBeat::Execute(NetClient* pClient, VariantVector& data)
{
    if(!pClient || data.size() < 2) {
        return;
    }

    GetMasterBroadway()->SetGlobalOnlineCount(data[1].GetUINT());
}
