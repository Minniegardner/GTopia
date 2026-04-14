#include "TCPEventHeartBeat.h"
#include "Server/MasterBroadway.h"
#include "Server/GameServer.h"

void TCPEventHeartBeat::Execute(NetClient* pClient, VariantVector& data)
{
    if(!pClient || data.size() < 2) {
        return;
    }

    GetMasterBroadway()->SetGlobalOnlineCount(data[1].GetUINT());

    if(data.size() < 5) {
        return;
    }

    const uint32 epochDay = data[2].GetUINT();
    const uint32 eventType = data[3].GetUINT();
    const uint32 eventSeed = data[4].GetUINT();

    const bool changed = GetMasterBroadway()->SetDailyEventState(epochDay, eventType, eventSeed);
    if(changed) {
        GetGameServer()->OnDailyEventSync(epochDay, eventType, eventSeed, false);
    }
}
