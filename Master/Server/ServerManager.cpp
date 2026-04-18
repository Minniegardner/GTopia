#include "ServerManager.h"
#include "Utils/Timer.h"
#include "IO/Log.h"
#include "../Context.h"
#include "../World/WorldManager.h"

namespace {
const uint32 kDailyRoleEventTypes[] = {
    TCP_DAILY_EVENT_ROLE_JACK,
    TCP_DAILY_EVENT_ROLE_FARMER,
    TCP_DAILY_EVENT_ROLE_BUILDER,
    TCP_DAILY_EVENT_ROLE_SURGEON,
    TCP_DAILY_EVENT_ROLE_FISHER,
    TCP_DAILY_EVENT_ROLE_CHEF,
    TCP_DAILY_EVENT_ROLE_STAR_CAPTAIN
};

struct DailyQuestTemplate
{
    uint32 questItemOneID;
    uint32 questItemTwoID;
    uint32 questItemOneAmount;
    uint32 questItemTwoAmount;
    uint32 rewardOneID;
    uint32 rewardOneAmount;
    uint32 rewardTwoID;
    uint32 rewardTwoAmount;
};

const DailyQuestTemplate kDailyQuestTemplates[] = {
    {2, 4, 20, 10, 112, 1, 7188, 1},
    {24, 26, 15, 8, 224, 2, 1796, 1},
    {4584, 4586, 6, 6, 1362, 3, 238, 2},
    {5666, 5668, 5, 5, 916, 1, 138, 2},
    {748, 750, 12, 12, 242, 2, 340, 2}
};

uint32 WrapWeekDay(uint32 day)
{
    return day % 7;
}

uint32 BuildMonthlyEventDay(uint32 seed, uint32 salt)
{
    return 1 + ((seed + (salt * 1103515245u)) % 28);
}

const char* GetDailyEventName(uint32 eventType)
{
    switch(eventType) {
        case TCP_DAILY_EVENT_ROLE_JACK: return "Jack of all Trades Day";
        case TCP_DAILY_EVENT_ROLE_FARMER: return "Farmer Day";
        case TCP_DAILY_EVENT_ROLE_BUILDER: return "Builder Day";
        case TCP_DAILY_EVENT_ROLE_SURGEON: return "Surgeon Day";
        case TCP_DAILY_EVENT_ROLE_FISHER: return "Fishing Day";
        case TCP_DAILY_EVENT_ROLE_CHEF: return "Chef Day";
        case TCP_DAILY_EVENT_ROLE_STAR_CAPTAIN: return "Star Captain's Day";
        default: return "None";
    }
}

uint16 ResolveConfigServerPort(uint32 serverID)
{
    GameConfig* pCfg = GetContext()->GetGameConfig();
    if(!pCfg) {
        return 0;
    }

    for(const auto& server : pCfg->servers) {
        if(server.id == serverID && server.udpPort != 0) {
            return server.udpPort;
        }
    }

    if(!pCfg->servers.empty() && pCfg->servers[0].udpPort != 0) {
        return (uint16)(pCfg->servers[0].udpPort + serverID);
    }

    return 0;
}
}


#include "../Event/TCP/TCPEventHello.h"
#include "../Event/TCP/TCPEventAuth.h"
#include "../Event/TCP/TCPEventPlayerSession.h"
#include "../Event/TCP/TCPEventWorldInit.h"
#include "../Event/TCP/TCPEventRenderWorld.h"
#include "../Event/TCP/TCPEventHeartBeat.h"
#include "../Event/TCP/TCPEventWorldSendPlayer.h"
#include "../Event/TCP/TCPEventPlayerEndSession.h"
#include "../Event/TCP/TCPEventCrossServerAction.h"
#include "../Event/TCP/TCPEventKillServer.h"

ServerManager::ServerManager()
{
}

ServerManager::~ServerManager()
{
    Kill();
}

void ServerManager::OnClientConnect(NetClient* pClient)
{
    if(!pClient) {
        return;
    }
}

void ServerManager::OnClientDisconnect(NetClient* pClient)
{
    if(!pClient) {
        return;
    }

    NetServerInfo* pClientInfo = (NetServerInfo*)pClient->data;
    if(pClientInfo && pClientInfo->serverID != 0) {
        RemoveServer(pClientInfo->serverID);
    }

    delete static_cast<NetServerInfo*>(pClient->data);
    pClient->data = nullptr;
}

void ServerManager::UpdateTCPLogic(uint64 maxTimeMS)
{
    UpdateDailyEventState();

    uint64 startTime = Time::GetSystemTime();
    TCPPacketEvent event;

    uint32 processed = 0;

    while(m_packetQueue.try_dequeue(event)) {
        if(!event.pClient) {
            continue;
        }

        int8 type = event.data[0].GetINT();
        LOGGER_LOG_DEBUG("GOT TCP PACKET %d", type);

        switch(type) {
            case TCP_PACKET_HELLO: 
            case TCP_PACKET_AUTH: {
                m_events.Dispatch(type, event.pClient, event.data);
                break;
            }

            default: {
                if(!((NetServerInfo*)(event.pClient->data))->authed) {
                    LOGGER_LOG_WARN("Client trying to access un-authorized packets?! CLOSING!");
                    event.pClient->status = SOCKET_CLIENT_CLOSE;
                    continue;
                }

                m_events.Dispatch(type, event.pClient, event.data);
            }
        }

        processed++;
        if(Time::GetSystemTime() - startTime >= maxTimeMS) {
            break;
        }
    }

    std::vector<uint16> staleServers;
    for(auto& [serverID, pServer] : m_servers) {
        if(!pServer) {
            staleServers.push_back(serverID);
            continue;
        }

        NetClient* pClient = m_pNetSocket->GetClient(pServer->socketConnID);
        NetServerInfo* pClientInfo = pClient ? (NetServerInfo*)pClient->data : nullptr;
        if(!pClient || !pClientInfo || pClientInfo->lastHeartbeatTime.GetElapsedTime() >= 15000) {
            staleServers.push_back(serverID);
        }
    }

    for(auto serverID : staleServers) {
        LOGGER_LOG_WARN("Server %d timed out, removing it from master cache", serverID);
        RemoveServer(serverID);
    }

    if(processed > 0) {
        LOGGER_LOG_DEBUG("Processed %d TCP packets maxMS %d, took %d MS", processed, maxTimeMS, Time::GetSystemTime() - startTime);
    }
}

void ServerManager::Kill()
{
    while (!m_servers.empty()) {
        RemoveServer(m_servers.begin()->first);
    }

    ServerBroadwayBase::Kill();
    m_servers.clear();
}

void ServerManager::RegisterEvents()
{
    ServerBroadwayBase::RegisterEvents();

    RegisterEvent<TCPEventHello>(TCP_PACKET_HELLO);
    RegisterEvent<TCPEventAuth>(TCP_PACKET_AUTH);
    RegisterEvent<TCPEventPlayerSession>(TCP_PACKET_PLAYER_CHECK_SESSION);
    RegisterEvent<TCPEventWorldInit>(TCP_PACKET_WORLD_INIT);
    RegisterEvent<TCPEventRenderWorld>(TCP_PACKET_RENDER_WORLD);
    RegisterEvent<TCPEventHeartBeat>(TCP_PACKET_HEARTBEAT);
    RegisterEvent<TCPEventWorldSendPlayer>(TCP_PACKET_WORLD_SEND_PLAYER);
    RegisterEvent<TCPEventPlayerEndSession>(TCP_PACKET_PLAYER_END_SESSION);
    RegisterEvent<TCPEventCrossServerAction>(TCP_PACKET_CROSS_SERVER_ACTION);
    RegisterEvent<TCPEventKillServer>(TCP_PACKET_KILL_SERVER);
}

ServerInfo* ServerManager::GetServerByID(uint16 serverID)
{
    auto it = m_servers.find(serverID);
    if(it == m_servers.end()) {
        return nullptr;
    }

    return it->second;
}

bool ServerManager::SendPacketRaw(uint16 serverID, VariantVector& data)
{
    if(!m_pNetSocket) {
        return false;
    }

    ServerInfo* pServer = GetServerByID(serverID);
    if(!pServer) {
        return false;
    }

    NetClient* pNetClient = m_pNetSocket->GetClient(pServer->socketConnID);
    if(!pNetClient) {
        return false;
    }

    return pNetClient->Send(data);
}

uint32 ServerManager::GetTotalOnlineCount() const
{
    uint32 totalOnlineCount = 0;

    for(auto& [_, pServer] : m_servers) {
        if(!pServer) {
            continue;
        }

        totalOnlineCount += pServer->playerCount;
    }

    return totalOnlineCount;
}

void ServerManager::SendWorldPlayerFailPacket(int32 playerNetID, uint32 serverID, const string& reason)
{
    VariantVector data(4);

    data[0] = TCP_PACKET_WORLD_SEND_PLAYER;
    data[1] = TCP_RESULT_FAIL;
    data[2] = playerNetID;
    data[3] = reason;

    SendPacketRaw(serverID, data);
}

void ServerManager::SendWorldPlayerSuccessPacket(int32 playerNetID, uint32 serverID, uint32 worldID, const string& serverIP, uint16 serverPort, const string& worldName, uint32 userID, uint32 loginToken, uint32 serverIDForPacket)
{
    if(serverPort == 0) {
        serverPort = ResolveConfigServerPort(serverID);
    }

    VariantVector data(10);

    data[0] = TCP_PACKET_WORLD_SEND_PLAYER;
    data[1] = TCP_RESULT_OK;
    data[2] = playerNetID;
    data[3] = serverID;
    data[4] = worldID;
    data[5] = serverIP;
    data[6] = serverPort;
    data[7] = worldName;
    data[8] = userID;
    data[9] = loginToken;

    SendPacketRaw(serverIDForPacket, data);
}

void ServerManager::SendWorldInitPacket(const string& worldName, uint32 serverID)
{
    VariantVector data(2);

    data[0] = TCP_PACKET_WORLD_INIT;
    data[1] = worldName;

    SendPacketRaw(serverID, data);
}

void ServerManager::SendAuthPacket(bool succeed, uint32 serverID)
{
    VariantVector data(2);

    data[0] = TCP_PACKET_AUTH;
    data[1] = succeed;

    SendPacketRaw(serverID, data);
}

void ServerManager::SendRenderResult(int32 result, uint32 playerUserID, const string& worldName, uint32 serverID)
{
    VariantVector data(5);

    data[0] = TCP_PACKET_RENDER_WORLD;
    data[1] = TCP_RENDER_RESULT;
    data[2] = result;
    data[3] = playerUserID;
    data[4] = worldName;

    SendPacketRaw(serverID, data);
}

void ServerManager::SendRenderRequest(uint32 playerUserID, uint32 worldID, uint32 serverID)
{
    VariantVector data(4);

    data[0] = TCP_PACKET_RENDER_WORLD;
    data[1] = TCP_RENDER_REQUEST;
    data[2] = playerUserID;
    data[3] = worldID;

    SendPacketRaw(serverID, data);
}

void ServerManager::SendPlayerSessionCheck(bool hasSession, int32 playerNetID, int16 connectionID)
{
    NetClient* pClient = m_pNetSocket->GetClient(connectionID);
    if(!pClient) {
        return;
    }

    VariantVector data(3);
    
    data[0] = TCP_PACKET_PLAYER_CHECK_SESSION;
    data[1] = playerNetID;
    data[2] = hasSession;

    pClient->Send(data);
}

void ServerManager::SendHelloPacket(const string &authKey, int16 connectionID)
{
    NetClient* pClient = m_pNetSocket->GetClient(connectionID);
    if(!pClient) {
        return;
    }

    VariantVector data(2);
    
    data[0] = TCP_PACKET_HELLO;
    data[1] = authKey;

    pClient->Send(data);
}

void ServerManager::SendCrossServerActionResult(uint16 serverID, int32 actionType, uint32 sourceUserID, int32 resultCode, const string& targetName)
{
    VariantVector data(6);
    data[0] = TCP_PACKET_CROSS_SERVER_ACTION;
    data[1] = TCP_CROSS_ACTION_RESULT;
    data[2] = actionType;
    data[3] = sourceUserID;
    data[4] = resultCode;
    data[5] = targetName;

    SendPacketRaw(serverID, data);
}

bool ServerManager::SendCrossServerActionExecute(
    uint16 targetServerID,
    int32 actionType,
    uint32 targetUserID,
    uint32 sourceUserID,
    const string& sourceRawName,
    const string& payloadText,
    uint64 payloadNumber,
    const string& targetRawName)
{
    VariantVector data(10);
    data[0] = TCP_PACKET_CROSS_SERVER_ACTION;
    data[1] = TCP_CROSS_ACTION_EXECUTE;
    data[2] = actionType;
    data[3] = targetUserID;
    data[4] = sourceUserID;
    data[5] = sourceRawName;
    data[6] = payloadText;
    data[7] = (uint32)payloadNumber;
    data[8] = (uint32)targetServerID;
    data[9] = targetRawName;

    return SendPacketRaw(targetServerID, data);
}

bool ServerManager::SendCrossServerActionExecuteAll(
    int32 actionType,
    uint32 sourceUserID,
    const string& sourceRawName,
    const string& payloadText,
    uint64 payloadNumber,
    uint16 excludeServerID)
{
    bool sentAny = false;
    bool hasEligibleTarget = false;

    for(const auto& [serverID, pServer] : m_servers) {
        if(!pServer || pServer->serverType != CONFIG_SERVER_GAME) {
            continue;
        }

        if(excludeServerID != 0 && serverID == excludeServerID) {
            continue;
        }

        hasEligibleTarget = true;

        VariantVector data(10);
        data[0] = TCP_PACKET_CROSS_SERVER_ACTION;
        data[1] = TCP_CROSS_ACTION_EXECUTE;
        data[2] = actionType;
        data[3] = (uint32)0;
        data[4] = sourceUserID;
        data[5] = sourceRawName;
        data[6] = payloadText;
        data[7] = (uint32)payloadNumber;
        data[8] = (uint32)serverID;
        data[9] = string();

        if(SendPacketRaw(serverID, data)) {
            sentAny = true;
        }
    }

    return hasEligibleTarget ? sentAny : true;
}

void ServerManager::AddServer(uint16 serverID, NetClient* pClient, int8 serverType)
{
    if(!pClient) {
        return;
    }

    if(serverType != CONFIG_SERVER_GAME && serverType != CONFIG_SERVER_RENDERER) {
        pClient->status = SOCKET_CLIENT_CLOSE;
        LOGGER_LOG_WARN("Unknown server %d type %d, closing", serverID, serverType);
        return;
    }

    auto it = m_servers.find(serverID);
    if(serverID == 0|| it != m_servers.end()) {
        pClient->status = SOCKET_CLIENT_CLOSE;
        LOGGER_LOG_ERROR("Server %d already exists but we tried to add it again??", serverID);
        return;
    }

    GameConfig* pCfg = GetContext()->GetGameConfig();
    if(!pCfg || serverID >= pCfg->servers.size()) {
        pClient->status = SOCKET_CLIENT_CLOSE;
        LOGGER_LOG_ERROR("Server %d is out of range in servers config (size=%d)", serverID, pCfg ? (int32)pCfg->servers.size() : -1);
        return;
    }

    auto serverNetInfo = pCfg->servers[serverID];
    if(serverNetInfo.serverType != serverType || serverNetInfo.lanIP.empty() || serverNetInfo.wanIP.empty() || serverNetInfo.udpPort == 0) {
        pClient->status = SOCKET_CLIENT_CLOSE;
        LOGGER_LOG_ERROR("Invalid config for server %d (type=%d lan='%s' wan='%s' udp=%d)", serverID, serverNetInfo.serverType, serverNetInfo.lanIP.c_str(), serverNetInfo.wanIP.c_str(), serverNetInfo.udpPort);
        return;
    }

    string serverTypeStr = "game";
    if(serverType == CONFIG_SERVER_RENDERER) serverTypeStr = "renderer";

    LOGGER_LOG_INFO("Server %d (%s) added to cache %s:%d", serverID, serverTypeStr.c_str(), serverNetInfo.wanIP.c_str(), serverNetInfo.udpPort);
    ServerInfo* pServer = new ServerInfo();
    pServer->serverID = serverID;
    pServer->socketConnID = pClient->connectionID;
    pServer->wanIP = serverNetInfo.wanIP;
    pServer->port = serverNetInfo.udpPort;
    pServer->serverType = (eConfigServerType)serverType;

    m_servers.insert_or_assign(pServer->serverID, pServer);
}

void ServerManager::RemoveServer(uint16 serverID)
{
    auto it = m_servers.find(serverID);
    if(it == m_servers.end()) {
        return;
    }

    ServerInfo* pServer = it->second;

    NetClient* pClient = m_pNetSocket ? m_pNetSocket->GetClient(pServer->socketConnID) : nullptr;
    if(pClient) {
        NetServerInfo* pNetInfo = static_cast<NetServerInfo*>(pClient->data);
        SAFE_DELETE(pNetInfo);
        pClient->data = nullptr;
        pClient->status = SOCKET_CLIENT_CLOSE;
    }

    SAFE_DELETE(pServer);
    m_servers.erase(it);

    // Clear all world contexts pinned to dead server to avoid stale join routing.
    GetWorldManager()->RemoveWorldsWithServerID(serverID);
}

ServerInfo* ServerManager::GetBestGameServer()
{
    ServerInfo* pBestServer = nullptr;
    float bestScore = 999999999.0f;

    for(auto& [_, pServer] : m_servers) {
        if(!pServer || pServer->serverType != CONFIG_SERVER_GAME) {
            continue;
        }

        float score = static_cast<float>(pServer->playerCount) / static_cast<float>(pServer->worldCount + 1);
        if(score < bestScore) {
            pBestServer = pServer;
            bestScore = score;
        }
    }

    return pBestServer;
}

ServerInfo* ServerManager::GetBestRenderServer()
{
    for(auto& [_, pServer] : m_servers) {
        if(!pServer || pServer->serverType != CONFIG_SERVER_RENDERER) {
            continue;
        }

        return pServer;
    }

    return nullptr;
}

bool ServerManager::HasAnyGameServer()
{
    for(auto& [_, pServer] : m_servers) {
        if(!pServer || pServer->serverType != CONFIG_SERVER_GAME) {
            continue;
        }

        return true;
    }

    return false;
}

void ServerManager::UpdateDailyEventState()
{
    const uint64 epochSec = Time::GetTimeSinceEpoch();
    const uint32 epochDay = (uint32)(epochSec / 86400ull);

    if(epochDay == 0 || epochDay == m_dailyEpochDay) {
        return;
    }

    m_dailyEpochDay = epochDay;
    m_dailyEventSeed = (epochDay * 1103515245u) + 12345u;

    const size_t eventCount = sizeof(kDailyRoleEventTypes) / sizeof(kDailyRoleEventTypes[0]);
    const uint32 eventIndex = m_dailyEventSeed % (uint32)eventCount;
    m_dailyEventType = kDailyRoleEventTypes[eventIndex];

    const size_t questCount = sizeof(kDailyQuestTemplates) / sizeof(kDailyQuestTemplates[0]);
    const DailyQuestTemplate& questTemplate = kDailyQuestTemplates[m_dailyEventSeed % (uint32)questCount];
    m_dailyQuestData.questItemOneID = questTemplate.questItemOneID;
    m_dailyQuestData.questItemTwoID = questTemplate.questItemTwoID;
    m_dailyQuestData.questItemOneAmount = questTemplate.questItemOneAmount;
    m_dailyQuestData.questItemTwoAmount = questTemplate.questItemTwoAmount;
    m_dailyQuestData.rewardOneID = questTemplate.rewardOneID;
    m_dailyQuestData.rewardOneAmount = questTemplate.rewardOneAmount;
    m_dailyQuestData.rewardTwoID = questTemplate.rewardTwoID;
    m_dailyQuestData.rewardTwoAmount = questTemplate.rewardTwoAmount;

    const uint32 weekAnchor = WrapWeekDay(epochDay);
    m_weeklyEventsData.roleQuestFarmerDay = WrapWeekDay(weekAnchor + 0);
    m_weeklyEventsData.roleQuestBuilderDay = WrapWeekDay(weekAnchor + 1);
    m_weeklyEventsData.roleQuestSurgeonDay = WrapWeekDay(weekAnchor + 2);
    m_weeklyEventsData.roleQuestFishingDay = WrapWeekDay(weekAnchor + 3);
    m_weeklyEventsData.roleQuestChefDay = WrapWeekDay(weekAnchor + 4);
    m_weeklyEventsData.roleQuestCaptainDay = WrapWeekDay(weekAnchor + 5);

    m_monthlyEventsData.lockeDayOne = BuildMonthlyEventDay(m_dailyEventSeed, 1);
    m_monthlyEventsData.lockeDayTwo = BuildMonthlyEventDay(m_dailyEventSeed, 2);
    m_monthlyEventsData.carnivalDayOne = BuildMonthlyEventDay(m_dailyEventSeed, 3);
    m_monthlyEventsData.carnivalDayTwo = BuildMonthlyEventDay(m_dailyEventSeed, 4);
    m_monthlyEventsData.surgeryDay = BuildMonthlyEventDay(m_dailyEventSeed, 5);
    m_monthlyEventsData.allHowlsEveDay = BuildMonthlyEventDay(m_dailyEventSeed, 6);
    m_monthlyEventsData.geigerDay = BuildMonthlyEventDay(m_dailyEventSeed, 7);
    m_monthlyEventsData.ghostDay = BuildMonthlyEventDay(m_dailyEventSeed, 8);
    m_monthlyEventsData.mutantKitchenDay = BuildMonthlyEventDay(m_dailyEventSeed, 9);
    m_monthlyEventsData.voucherDayz = BuildMonthlyEventDay(m_dailyEventSeed, 10);

    LOGGER_LOG_INFO("Daily event updated: epoch_day=%u event=%u (%s) seed=%u", m_dailyEpochDay, m_dailyEventType, GetDailyEventName(m_dailyEventType), m_dailyEventSeed);
}

ServerManager* GetServerManager() { return ServerManager::GetInstance(); }
