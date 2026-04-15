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
        LOGGER_LOG_WARN("PLAYER_SWITCH missing session userID=%u targetServer=%u", eventData.userID, eventData.targetServerID);
        return;
    }

    const uint16 previousServerID = pPlayer->serverID;
    pPlayer->serverID = (uint16)eventData.targetServerID;
    pPlayer->loginTime = Time::GetTimeSinceEpoch();

    LOGGER_LOG_INFO("PLAYER_SWITCH userID=%u server %u -> %u", eventData.userID, previousServerID, pPlayer->serverID);
}