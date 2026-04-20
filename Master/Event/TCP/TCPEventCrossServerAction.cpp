#include "TCPEventCrossServerAction.h"
#include "../../Server/ServerManager.h"
#include "../../Server/GameServer.h"
#include "Utils/StringUtils.h"

void TCPEventCrossServerAction::Execute(NetClient* pClient, VariantVector& data)
{
    if(!pClient || data.size() < 8) {
        return;
    }

    const int32 packetSubType = data[1].GetINT();
    if(packetSubType != TCP_CROSS_ACTION_REQUEST) {
        if(packetSubType == TCP_CROSS_ACTION_RESULT && data.size() >= 8) {
            const uint32 sourceServerID = data.size() >= 8 ? data[7].GetUINT() : 0;
            if(sourceServerID != 0) {
                GetServerManager()->SendPacketRaw((uint16)sourceServerID, data);
            }
        }

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

    if(actionType == TCP_CROSS_ACTION_SERVER_SWITCH) {
        uint32 targetServerID = 0;
        if(payloadText.empty() || ToUInt(payloadText, targetServerID) != TO_INT_SUCCESS || targetServerID == 0) {
            GetServerManager()->SendCrossServerActionResult(sourceServerID, actionType, sourceUserID, TCP_CROSS_ACTION_RESULT_NOT_FOUND, payloadText);
            return;
        }

        ServerInfo* pTargetServer = GetServerManager()->GetServerByID((uint16)targetServerID);
        if(!pTargetServer || pTargetServer->serverType != CONFIG_SERVER_GAME) {
            GetServerManager()->SendCrossServerActionResult(sourceServerID, actionType, sourceUserID, TCP_CROSS_ACTION_RESULT_NOT_FOUND, payloadText);
            return;
        }

        const string redirectPayload = pTargetServer->wanIP + "|" + ToString(pTargetServer->port) + "|" + ToString((uint32)pTargetServer->serverID);

        if(!GetServerManager()->SendCrossServerActionExecute(
            (uint16)sourceServerID,
            actionType,
            sourceUserID,
            sourceUserID,
            sourceRawName,
            redirectPayload,
            0,
            sourceRawName,
            sourceServerID)) {
            GetServerManager()->SendCrossServerActionResult(sourceServerID, actionType, sourceUserID, TCP_CROSS_ACTION_RESULT_SEND_FAILED, ToString(targetServerID));
            return;
        }

        GetServerManager()->SendCrossServerActionResult(sourceServerID, actionType, sourceUserID, TCP_CROSS_ACTION_RESULT_OK, "S" + ToString(targetServerID));
        return;
    }

    if(
        actionType == TCP_CROSS_ACTION_SUPER_BROADCAST ||
        actionType == TCP_CROSS_ACTION_MAINTENANCE ||
        actionType == TCP_CROSS_ACTION_SUMMON_ALL ||
        actionType == TCP_CROSS_ACTION_GLOBAL_SFX
    ) {
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
        string details = targetQuery + "|";
        const size_t limit = std::min<size_t>(matches.size(), 8);
        for(size_t i = 0; i < limit; ++i) {
            PlayerSession* pSession = matches[i];
            if(!pSession) {
                continue;
            }

            if(i != 0) {
                details += ", ";
            }

            details += pSession->name;
            details += "(#" + ToString(pSession->userID) + ")";
        }

        GetServerManager()->SendCrossServerActionResult(sourceServerID, actionType, sourceUserID, TCP_CROSS_ACTION_RESULT_MULTIPLE_MATCH, details);
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
        pTargetSession->name,
        sourceServerID)) {
        GetServerManager()->SendCrossServerActionResult(sourceServerID, actionType, sourceUserID, TCP_CROSS_ACTION_RESULT_SEND_FAILED, pTargetSession->name);
        return;
    }

    GetServerManager()->SendCrossServerActionResult(sourceServerID, actionType, sourceUserID, TCP_CROSS_ACTION_RESULT_OK, pTargetSession->name);
}
