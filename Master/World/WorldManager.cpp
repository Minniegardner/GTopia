#include "WorldManager.h"
#include "Utils/StringUtils.h"
#include "Database/Table/WorldDBTable.h"
#include "../Context.h"
#include "../Server/ServerManager.h"
#include "../Server/GameServer.h"

WorldManager::WorldManager()
{
}

WorldManager::~WorldManager()
{
}

void WorldManager::OnHandleDatabase(QueryTaskResult&& result)
{
    int32 state = result.extraData[0].GetINT();

    switch (state) {
        case WORLD_DB_STATE_CHECK_EXISTS: {
            HandleDBWorldExists(std::move(result));
            break;
        }

        case WORLD_DB_STATE_CREATE: {
            HandleDBWorldCreate(std::move(result));
            break;
        }
    }

    result.Destroy();
}

void WorldManager::OnHandleTCP(VariantVector&& result)
{
    if(result.empty()) {
        return;
    }

    int32 packetType = result[0].GetINT();

    switch (packetType) {
        case TCP_PACKET_WORLD_INIT: {
            HandleWorldInit(std::move(result));
            break;
        }
        case TCP_PACKET_WORLD_SEND_PLAYER: {
            HandlePlayerJoinRequest(std::move(result));
            break;
        }
    }
}

void WorldManager::HandleWorldInit(VariantVector&& result)
{
    if(result.size() < 3) {
        return;
    }

    int32 initResult = result[1].GetINT();
    uint32 worldID = result[2].GetUINT();

    WorldSession* pWorld = GetWorldByID(worldID);
    if(!pWorld) {
        return;
    }

    ServerManager* pServerMgr = GetServerManager();

    if(initResult != TCP_RESULT_OK) {
        for(auto& pending : pWorld->pendingPlayers) {
            pServerMgr->SendWorldPlayerFailPacket(pending.playerNetID, pending.serverID);
        }

        m_worldSessions.erase(worldID);
        return;
    }

    pWorld->state = WORLD_STATE_ON;

    ServerInfo* pServer = pServerMgr->GetServerByID(pWorld->serverID);
    if(!pServer) {
        return;
    }

    for(auto& pending : pWorld->pendingPlayers) {
        PlayerSession* pSession = GetGameServer()->GetPlayerSessionByUserID(pending.userID);
        if(pSession) {
            pSession->serverID = (uint16)pWorld->serverID;
        }

        pServerMgr->SendWorldPlayerSuccessPacket(
            pending.playerNetID,
            pWorld->serverID,
            worldID,
            pServer->wanIP,
            pServer->port,
            pending.serverID
        );
    }

    pWorld->pendingPlayers.clear();
}

void WorldManager::HandlePlayerJoinRequest(VariantVector&& result)
{
    if(result.size() < 5) {
        return;
    }

    uint32 serverID = result[1].GetUINT();
    int32 playerNetID = result[2].GetINT();
    string upperWorldName = ToUpper(result[3].GetString());
    uint32 userID = result[4].GetUINT();

    WorldSession* pWorld = GetWorldByName(upperWorldName);
    if(!pWorld) {
        QueryRequest req = MakeWorldExistsByName(upperWorldName, GetNetID());
        req.extraData.resize(5);
        req.extraData[0] = WORLD_DB_STATE_CHECK_EXISTS;
        req.extraData[1] = (uint32)serverID;
        req.extraData[2] = playerNetID;
        req.extraData[3] = upperWorldName;
        req.extraData[4] = userID;

        DatabaseWorldExec(GetContext()->GetDatabasePool(), DB_WORLD_EXISTS_BY_NAME, req);
    }
    else if(pWorld->state == WORLD_STATE_LOADING) {
        pWorld->AddPending(serverID, playerNetID, userID);
    }
    else if(pWorld->state == WORLD_STATE_ON) {
        ServerInfo* pServer = GetServerManager()->GetServerByID(pWorld->serverID);
        if(!pServer) {
            GetServerManager()->SendWorldPlayerFailPacket(playerNetID, serverID);
            return;
        }

        PlayerSession* pSession = GetGameServer()->GetPlayerSessionByUserID(userID);
        if(pSession) {
            pSession->serverID = (uint16)pWorld->serverID;
        }

        GetServerManager()->SendWorldPlayerSuccessPacket(
            playerNetID,
            pWorld->serverID,
            pWorld->worldID,
            pServer->wanIP,
            pServer->port,
            serverID
        );
    }
}

void WorldManager::HandleDBWorldExists(QueryTaskResult&& result)
{
    if(result.extraData.size() < 5) {
        return;
    }

    uint32 serverID = result.extraData[1].GetUINT();
    int32 playerNetID = result.extraData[2].GetINT();
    uint32 userID = result.extraData[4].GetUINT();

    ServerManager* pServerMgr = GetServerManager();
    if(!result.result) {
        pServerMgr->SendWorldPlayerFailPacket(playerNetID, serverID);
        return;
    }

    string worldName = result.extraData[3].GetString();

    if(result.result->GetRowCount() == 0) {
        QueryRequest req = MakeWorldCreate(result.extraData[3].GetString(), GetNetID());
        req.extraData = std::move(result.extraData);
        req.extraData[0] = WORLD_DB_STATE_CREATE;

        DatabaseWorldExec(GetContext()->GetDatabasePool(), DB_WORLD_CREATE, req);
        return;
    }

    uint32 worldID = result.result->GetField("ID", 0).GetUINT();

    CreateWorldSessionAndNotice(worldID, worldName, playerNetID, serverID, userID);
}

void WorldManager::HandleDBWorldCreate(QueryTaskResult&& result)
{
    if(result.extraData.size() < 5) {
        return;
    }

    uint32 serverID = result.extraData[1].GetUINT();
    int32 playerNetID = result.extraData[2].GetINT();
    string worldName = result.extraData[3].GetString();
    uint32 userID = result.extraData[4].GetUINT();
    uint32 worldID = result.increment;

    CreateWorldSessionAndNotice(worldID, worldName, playerNetID, serverID, userID);
}

void WorldManager::CreateWorldSessionAndNotice(uint32 worldID, const string& worldName, int32 playerNetID, uint32 serverID, uint32 userID)
{
    ServerManager* pServerMgr = GetServerManager();
    ServerInfo* pServer = pServerMgr->GetBestGameServer();
    if(!pServer) {
        pServerMgr->SendWorldPlayerFailPacket(playerNetID, serverID);
        return;
    }

    WorldSession worldSession;

    worldSession.worldID = worldID;
    worldSession.state = WORLD_STATE_LOADING;
    worldSession.serverID = pServer->serverID;
    worldSession.worldName = worldName;
    worldSession.AddPending(serverID, playerNetID, userID);

    m_worldSessions.insert_or_assign(worldSession.worldID, worldSession);
    pServerMgr->SendWorldInitPacket(worldName, pServer->serverID);
}

WorldSession* WorldManager::GetWorldByName(const string& worldName)
{
    string searchName = ToLower(worldName);
    for(auto& [_, worldSession] : m_worldSessions) {
        if(ToLower(worldSession.worldName) == searchName) {
            return &worldSession;
        }
    }

    return nullptr;
}

WorldSession* WorldManager::GetWorldByID(uint32 worldID)
{
    auto it = m_worldSessions.find(worldID);
    if(it != m_worldSessions.end()) {
        return &it->second;
    }

    return nullptr;
}

void WorldManager::RemoveWorldsWithServerID(uint32 serverID)
{
    for(auto it = m_worldSessions.begin(); it != m_worldSessions.end();) {
        if(it->second.serverID == serverID) {
            it = m_worldSessions.erase(it);
            continue;
        }

        ++it;
    }
}

WorldManager* GetWorldManager() { return WorldManager::GetInstance(); }