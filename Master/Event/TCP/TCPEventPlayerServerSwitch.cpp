#include "TCPEventPlayerServerSwitch.h"
#include "../../Server/GameServer.h"
#include "Utils/Timer.h"

void TCPPlayerServerSwitchEventData::FromVariant(VariantVector& varVec)
{
    if(varVec.size() < 3) {
        return;
    }

    userID = varVec[1].GetUINT();
    targetServerID = varVec[2].GetUINT();
}

void TCPEventPlayerServerSwitch::Execute(NetClient* pClient, VariantVector& data)
{
    TCPPlayerServerSwitchEventData eventData;
    eventData.FromVariant(data);

    PlayerSession* pPlayer = GetGameServer()->GetPlayerSessionByUserID(eventData.userID);
    if(!pPlayer) {
        return;
    }

    pPlayer->serverID = (uint16)eventData.targetServerID;
    pPlayer->loginTime = Time::GetTimeSinceEpoch();
}