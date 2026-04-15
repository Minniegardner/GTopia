#include "TCPEventPlayerSession.h"
#include "../../Server/GameServer.h"
#include "../../Server/ServerManager.h"

void TCPPlayerSessionEventData::FromVariant(VariantVector& varVec)
{
    if(varVec.size() < 5) {
        return;
    }

    netID = varVec[1].GetINT();
    userID = varVec[2].GetUINT();
    token = varVec[3].GetUINT();
    serverID = varVec[4].GetUINT();
}

void TCPEventPlayerSession::Execute(NetClient* pClient, VariantVector& data)
{
    TCPPlayerSessionEventData eventData;
    eventData.FromVariant(data);

    PlayerSession* pPlayer = GetGameServer()->GetPlayerSessionByUserID(eventData.userID);
    bool hasSession = true;

    if(!pPlayer) {
        hasSession = false;
    }
    else if(pPlayer->loginToken != eventData.token) {
        hasSession = false;
    }
    else if(pPlayer->serverID != eventData.serverID) {
        LOGGER_LOG_WARN("SESSION_CHECK server mismatch accepted userID=%u token=%u expectedServer=%u requestServer=%u", eventData.userID, eventData.token, pPlayer->serverID, eventData.serverID);
    }

    LOGGER_LOG_INFO("SESSION_CHECK userID=%u netID=%d requestServer=%u result=%d", eventData.userID, eventData.netID, eventData.serverID, hasSession ? 1 : 0);
    GetServerManager()->SendPlayerSessionCheck(hasSession, eventData.netID, pClient->connectionID);
}