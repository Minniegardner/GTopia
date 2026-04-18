#include "TCPEventCrossServerAction.h"
#include "../../Server/ServerManager.h"
#include "../../Server/GameServer.h"

void TCPEventCrossServerAction::Execute(NetClient* pClient, VariantVector& data)
{
    if(!pClient || data.size() < 9) {
        return;
    }

    const int32 packetSubType = data[1].GetINT();
    if(packetSubType != TCP_CROSS_ACTION_REQUEST) {
        return;
    }

    const int32 actionType = data[2].GetINT();
    const uint32 sourceServerID = data[3].GetUINT();
    const uint32 sourceUserID = data[4].GetUINT();
    const string sourceRawName = data[5].GetString();
    const string targetQuery = data[6].GetString();
    const bool exactMatch = data[7].GetBool();
    const string payloadText = data[8].GetString();
    const uint64 payloadNumber = data.size() >= 10 ? (uint64)data[9].GetUINT() : 0;

    if(actionType == TCP_CROSS_ACTION_SUPER_BROADCAST || actionType == TCP_CROSS_ACTION_MAINTENANCE) {
        if(payloadText.empty()) {
            GetServerManager()->SendCrossServerActionResult(sourceServerID, actionType, sourceUserID, TCP_CROSS_ACTION_RESULT_NOT_FOUND, "");
            return;
        }

        if(!GetServerManager()->SendCrossServerActionExecuteAll(actionType, sourceUserID, sourceRawName, payloadText, payloadNumber, (uint16)sourceServerID)) {
            GetServerManager()->SendCrossServerActionResult(sourceServerID, actionType, sourceUserID, TCP_CROSS_ACTION_RESULT_SEND_FAILED, "ALL");
            return;
        }

        GetServerManager()->SendCrossServerActionResult(sourceServerID, actionType, sourceUserID, TCP_CROSS_ACTION_RESULT_OK, "ALL");
        return;
    }

    if(targetQuery.empty()) {
        GetServerManager()->SendCrossServerActionResult(sourceServerID, actionType, sourceUserID, TCP_CROSS_ACTION_RESULT_NOT_FOUND, "");
        return;
    }

    auto matches = GetGameServer()->FindPlayerSessionsByNamePrefix(targetQuery, exactMatch);
    if(matches.empty()) {
        GetServerManager()->SendCrossServerActionResult(sourceServerID, actionType, sourceUserID, TCP_CROSS_ACTION_RESULT_NOT_FOUND, targetQuery);
        return;
    }

    if(!exactMatch && matches.size() > 1) {
        GetServerManager()->SendCrossServerActionResult(sourceServerID, actionType, sourceUserID, TCP_CROSS_ACTION_RESULT_MULTIPLE_MATCH, targetQuery);
        return;
    }

    PlayerSession* pTargetSession = matches[0];
    if(!pTargetSession) {
        GetServerManager()->SendCrossServerActionResult(sourceServerID, actionType, sourceUserID, TCP_CROSS_ACTION_RESULT_NOT_FOUND, "");
        return;
    }

    if(pTargetSession->userID == sourceUserID) {
        GetServerManager()->SendCrossServerActionResult(sourceServerID, actionType, sourceUserID, TCP_CROSS_ACTION_RESULT_SELF_TARGET, pTargetSession->name);
        return;
    }

    if(!GetServerManager()->SendCrossServerActionExecute(
        pTargetSession->serverID,
        actionType,
        pTargetSession->userID,
        sourceUserID,
        sourceRawName,
        payloadText,
        payloadNumber,
        pTargetSession->name)) {
        GetServerManager()->SendCrossServerActionResult(sourceServerID, actionType, sourceUserID, TCP_CROSS_ACTION_RESULT_SEND_FAILED, pTargetSession->name);
        return;
    }

    GetServerManager()->SendCrossServerActionResult(sourceServerID, actionType, sourceUserID, TCP_CROSS_ACTION_RESULT_OK, pTargetSession->name);
}
