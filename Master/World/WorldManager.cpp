#include "WorldManager.h"
#include "Utils/StringUtils.h"
#include "Database/Table/WorldDBTable.h"
#include "../Context.h"
#include "../Server/ServerManager.h"
#include "../Player/PlayerManager.h"

WorldManager::WorldManager()
{
}

WorldManager::~WorldManager()
{
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
            ServerInfo* pServer = pServerMgr->GetServerByID(pending.serverID);
            if(!pServer) {
                continue;
            }

            pServerMgr->SendWorldPlayerFailPacket(pServer, pending.playerUserID);
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
        ServerInfo* pRoutedServer = pServerMgr->GetServerByID(pending.serverID);
        if(!pRoutedServer) {
            continue;
        }

        PlayerSession* pPlayerSession = GetPlayerManager()->GetSessionByID(pending.playerUserID);
        if(!pPlayerSession) {
            continue;
        }

        pPlayerSession->worldName = pWorld->worldName;
        pPlayerSession->serverID = pWorld->serverID;

        pServerMgr->SendWorldPlayerSuccessPacket(
            pRoutedServer,
            pending.playerUserID,
            pWorld->serverID,
            worldID,
            pServer->wanIP,
            pServer->port
        );
    }

    pWorld->pendingPlayers.clear();
}

void WorldManager::CheckWorldExistCB(QueryTaskResult&& result)
{
    Variant* pServerID = result.GetExtraData(0);
    Variant* pPlayerID = result.GetExtraData(1);

    if(!pServerID || !pPlayerID) {
        return;
    }

    Variant* pWorldName = result.GetExtraData(2);

    ServerInfo* pServer = GetServerManager()->GetServerByID(pServerID->GetUINT());
    if(!pServer) {
        return;
    }

    if(!result.result || !pWorldName) {
        GetServerManager()->SendWorldPlayerFailPacket(pServer, pPlayerID->GetINT());
        return;
    }

    if(result.result->GetRowCount() > 0) {
        Variant* pID = result.result->GetFieldSafe("ID", 0);
        if(!pID) {
            GetServerManager()->SendWorldPlayerFailPacket(pServer, pPlayerID->GetINT());
            return;
        }

        GetWorldManager()->CreateWorldSessionAndNotice(pID->GetUINT(), pWorldName->GetString(), pPlayerID->GetINT(), pServerID->GetUINT());
        return;
    }

    QueryRequest req = WorldDB::Create(pWorldName->GetString());
    req.extraData = std::move(result.extraData);
    req.callback = &WorldManager::CreateWorldCB;

    DatabaseWorldExec(GetContext()->GetDatabasePool(), req);
}

void WorldManager::CreateWorldCB(QueryTaskResult&& result)
{
    Variant* pServerID = result.GetExtraData(0);
    Variant* pPlayerID = result.GetExtraData(1);

    if(!pServerID || !pPlayerID) {
        return;
    }

    Variant* pWorldName = result.GetExtraData(2);

    ServerInfo* pServer = GetServerManager()->GetServerByID(pServerID->GetUINT());
    if(!pServer) {
        return;
    }

    if(result.increment == 0 || !pWorldName) {
        GetServerManager()->SendWorldPlayerFailPacket(pServer, pPlayerID->GetINT());
        return;
    }

    GetWorldManager()->CreateWorldSessionAndNotice(result.increment, pWorldName->GetString(), pPlayerID->GetINT(), pServerID->GetUINT());
}

void WorldManager::HandlePlayerJoinRequest(ServerInfo* pServer, VariantVector&& result)
{
    if(!pServer) {
        return;
    }

    if(result.size() < 3) {
        return;
    }

    int32 playerUserID = result[1].GetUINT();
    string upperWorldName = ToUpper(result[2].GetString());

    WorldSession* pWorld = GetWorldByName(upperWorldName);
    if(!pWorld) {
        QueryRequest req = WorldDB::ExistsByName(upperWorldName);
        req.AddExtraData((uint32)pServer->serverID, playerUserID, upperWorldName);
        req.callback = &WorldManager::CheckWorldExistCB;

        DatabaseWorldExec(GetContext()->GetDatabasePool(), req);
    }
    else if(pWorld->state == WORLD_STATE_LOADING) {
        pWorld->AddPending((uint32)pServer->serverID, playerUserID);
    }
    else if(pWorld->state == WORLD_STATE_ON) {
        ServerInfo* pTargetServer = GetServerManager()->GetServerByID(pWorld->serverID);
        if(!pTargetServer) {
            GetServerManager()->SendWorldPlayerFailPacket(pServer, playerUserID);
            return;
        }

        PlayerSession* pPlayerSession = GetPlayerManager()->GetSessionByID(playerUserID);
        if(!pPlayerSession) {
            return;
        }

        pPlayerSession->worldName = pWorld->worldName;
        pPlayerSession->serverID = pWorld->serverID;

        GetServerManager()->SendWorldPlayerSuccessPacket(
            pTargetServer,
            playerUserID,
            pWorld->serverID,
            pWorld->worldID,
            pTargetServer->wanIP,
            pTargetServer->port
        );
    }
}

void WorldManager::CreateWorldSessionAndNotice(uint32 worldID, const string& worldName, uint32 playerUserID, uint32 serverID)
{
    ServerManager* pServerMgr = GetServerManager();
    ServerInfo* pServer = pServerMgr->GetBestGameServer();

    if(!pServer) {
        ServerInfo* pRoutedServer = GetServerManager()->GetServerByID(serverID);
        if(!pRoutedServer) {
            return;
        }

        pServerMgr->SendWorldPlayerFailPacket(pRoutedServer, serverID);
        return;
    }

    WorldSession worldSession;

    worldSession.worldID = worldID;
    worldSession.state = WORLD_STATE_LOADING;
    worldSession.serverID = pServer->serverID;
    worldSession.worldName = worldName;
    worldSession.AddPending(serverID, playerUserID);

    m_worldSessions.insert_or_assign(worldSession.worldID, worldSession);
    pServerMgr->SendWorldInitPacket(pServer, worldName);
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