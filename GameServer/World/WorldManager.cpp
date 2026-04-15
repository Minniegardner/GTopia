#include "WorldManager.h"
#include "Algorithm/ChemsynthAlgorithm.h"
#include "../Context.h"
#include "../Server/MasterBroadway.h"
#include "../Server/GameServer.h"
#include "Database/Table/WorldDBTable.h"
#include "IO/File.h"

#include "../Event/UDP/GamePacket/ItemActivateRequest.h"
#include "../Event/UDP/GamePacket/PlayerCollect.h"
#include "../Event/UDP/GamePacket/UseDoorRequest.h"
#include "../Event/UDP/GamePacket/TileChangeRequest.h"
#include "../Event/UDP/GamePacket/State.h"
#include "../Event/UDP/GamePacket/PlayerSendIconState.h"
#include "../Event/UDP/GamePacket/BlockActivate.h"
#include "../Event/UDP/GamePacket/PlayerPunched.h"

WorldManager::WorldManager()
{
    RegisterEvents();
}

WorldManager::~WorldManager()
{
    Kill();
}

void WorldManager::OnHandleDatabase(QueryTaskResult&& result)
{
    if(!result.result) {
        return;
    }

    MasterBroadway* pMasterBroadway = GetMasterBroadway();
    uint32 worldID = result.result->GetField("ID", 0).GetUINT();

    string path = GetContext()->GetGameConfig()->worldSavePath
    + "/world_" + ToString(worldID) + ".bin";

    World* pWorld = nullptr;

    if(IsFileExists(path)) {
        File file;
        if(!file.Open(path)) {
            pMasterBroadway->SendWorldInitResult(false, worldID);
            result.Destroy();
            return;
        }

        uint32 fileSize = file.GetSize();
        uint8* pData = new uint8[fileSize];

        if(file.Read(pData, fileSize) != fileSize) {
            pMasterBroadway->SendWorldInitResult(false, worldID);
            file.Close();
            SAFE_DELETE_ARRAY(pData);
            result.Destroy();
            return;
        }

        MemoryBuffer memBuffer(pData, fileSize);

        pWorld = new World();
        pWorld->Serialize(memBuffer, false, true);
        file.Close();
        SAFE_DELETE_ARRAY(pData);
    }
    else {
        pWorld = new World();
        pWorld->GenerateWorld(WORLD_GENERATION_DEFAULT);
        pWorld->SetName(result.result->GetField("Name", 0).GetString());

        LOGGER_LOG_DEBUG("Saving new world %d, %s", worldID, path.c_str());

        File file;
        if(!file.Open(path, FILE_MODE_WRITE)) {
            LOGGER_LOG_WARN("Failed to save new world, hope to save in auto save");
        }
        else {
            MemoryBuffer memSize;
            pWorld->Serialize(memSize, true, true);
            
            uint32 worldMemSize = memSize.GetOffset();
            uint8* pWorldData = new uint8[worldMemSize];
        
            MemoryBuffer memBuffer(pWorldData, worldMemSize);
            pWorld->Serialize(memBuffer, true, true);

            file.Write(pWorldData, worldMemSize);
            file.Close();
            SAFE_DELETE_ARRAY(pWorldData);
        }
    }

    result.Destroy();

    pWorld->SetID(worldID);
    m_worlds.insert_or_assign(worldID, pWorld);

    pMasterBroadway->SendWorldInitResult(true, worldID);
}

void WorldManager::OnHandleTCP(VariantVector&& result)
{
    if(result.empty()) {
        return;
    }

    int32 packetType = result[0].GetINT();

    switch(packetType) {
        case TCP_PACKET_WORLD_INIT: {
            if(result.size() < 2) {
                return;
            }

            QueryRequest req = MakeGetWorldData(result[1].GetString(), GetNetID());
            DatabaseWorldExec(GetContext()->GetDatabasePool(), DB_WORLD_GET_DATA, req);
            break;
        }

        case TCP_PACKET_WORLD_SEND_PLAYER: {
            if(result.size() < 5) {
                return;
            }

            int32 oprResult = result[1].GetINT();
            int32 playerNetID = result[2].GetINT();

            LOGGER_LOG_INFO("WORLD_ROUTE recv result=%d playerNetID=%d payloadSize=%d onServer=%u", oprResult, playerNetID, (int32)result.size(), GetContext()->GetID());

            GamePlayer* pPlayer = GetGameServer()->GetPlayerByNetID(playerNetID);
            if(!pPlayer) {
                return;
            }

            if(oprResult != TCP_RESULT_OK) {
                LOGGER_LOG_WARN("WORLD_ROUTE fail playerUserID=%u playerNetID=%d", pPlayer->GetUserID(), playerNetID);
                pPlayer->SendOnFailedToEnterWorld();
                pPlayer->SendOnConsoleMessage("Unable to move you to this world, please try again later");
                return;
            }
            
            uint32 serverID = result[3].GetUINT();
            uint32 worldID = result[4].GetUINT();

            if(serverID == GetContext()->GetID()) {
                LOGGER_LOG_INFO("WORLD_ROUTE local join userID=%u worldID=%u serverID=%u", pPlayer->GetUserID(), worldID, serverID);
                World* pWorld = GetWorldByID(worldID);
                if(!pWorld) {
                    pPlayer->SendOnFailedToEnterWorld();
                    pPlayer->SendOnConsoleMessage("Unable to join world");
                    pPlayer->SetJoinWorld(false);
                    return;
                }

                if(!pWorld->PlayerJoinWorld(pPlayer)) {
                    pPlayer->SendOnFailedToEnterWorld();
                    pPlayer->SendOnConsoleMessage("Unable to join world");
                }
            }
            else {
                if(result.size() < 7) {
                    LOGGER_LOG_WARN("WORLD_ROUTE remote invalid payload userID=%u payloadSize=%d", pPlayer->GetUserID(), (int32)result.size());
                    pPlayer->SendOnFailedToEnterWorld();
                    pPlayer->SendOnConsoleMessage("Unable to switch you to another server, please try again later");
                    pPlayer->SetJoinWorld(false);
                    return;
                }

                const string targetServerIP = result[5].GetString();
                uint16 targetServerPort = (uint16)result[6].GetUINT();
                const string worldName = (result.size() >= 8) ? result[7].GetString() : "";

                if(targetServerPort == 0) {
                    for(const auto& serverCfg : GetContext()->GetGameConfig()->servers) {
                        if(serverCfg.id != (uint16)serverID || serverCfg.serverType != CONFIG_SERVER_GAME) {
                            continue;
                        }

                        targetServerPort = serverCfg.udpPort;
                        break;
                    }

                    LOGGER_LOG_WARN("WORLD_ROUTE remote port fallback userID=%u targetServer=%u fallbackPort=%u", pPlayer->GetUserID(), serverID, targetServerPort);
                }

                if(targetServerPort == 0) {
                    pPlayer->SendOnFailedToEnterWorld();
                    pPlayer->SendOnConsoleMessage("Unable to switch you to another server, invalid target port");
                    pPlayer->SetJoinWorld(false);
                    return;
                }

                LOGGER_LOG_INFO("WORLD_ROUTE remote redirect userID=%u worldID=%u worldName=%s fromServer=%u toServer=%u target=%s:%u", pPlayer->GetUserID(), worldID, worldName.c_str(), GetContext()->GetID(), serverID, targetServerIP.c_str(), targetServerPort);

                if(!GetMasterBroadway()->SendPlayerServerSwitch(pPlayer->GetUserID(), (uint16)serverID)) {
                    LOGGER_LOG_WARN("WORLD_ROUTE switch packet failed userID=%u targetServer=%u", pPlayer->GetUserID(), serverID);
                    pPlayer->SendOnFailedToEnterWorld();
                    pPlayer->SendOnConsoleMessage("Unable to switch you to another server, please try again later");
                    pPlayer->SetJoinWorld(false);
                    return;
                }

                const string doorID = pPlayer->ConsumePendingDoorWarpID();
                pPlayer->SetRedirectingSubServer(true);
                pPlayer->SendOnSendToServer(
                    targetServerPort,
                    pPlayer->GetLoginDetail().token,
                    pPlayer->GetUserID(),
                    targetServerIP,
                    worldName,
                    doorID,
                    LoginMode::REDIRECT_SUBSERVER_SILENT
                );

                PlayerLeaveWorld(pPlayer);
                pPlayer->SetJoinWorld(false);
            }

            break;
        }
    }
}

void WorldManager::PlayerJoinRequest(GamePlayer* pPlayer, const string& worldName)
{
    if(!pPlayer || pPlayer->IsJoiningWorld()) {
        return;
    }

    LOGGER_LOG_INFO("WORLD_JOIN request userID=%u world=%s serverID=%u", pPlayer->GetUserID(), worldName.c_str(), GetContext()->GetID());

    pPlayer->SetJoinWorld(true);
    World* pWorld = GetWorldByName(worldName);

    if(pWorld) {
        if(!pWorld->PlayerJoinWorld(pPlayer)) {
            pPlayer->SendOnFailedToEnterWorld();
        }

        pPlayer->SetJoinWorld(false);
        return;
    }

    GetMasterBroadway()->SendPlayerWorldJoin(pPlayer->GetNetID(), worldName);
}

void WorldManager::PlayerLeaveWorld(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    uint32 worldID = pPlayer->GetCurrentWorld();
    if(worldID == 0) {
        return;
    }

    World* pWorld = GetWorldByID(worldID);
    if(!pWorld) {
        return;
    }

    pWorld->PlayerLeaverWorld(pPlayer);
}

void WorldManager::Kill()
{
    for(auto& [_, pWorld] : m_worlds) {
        SAFE_DELETE(pWorld);
    }
}

World* WorldManager::GetWorldByID(uint32 worldID)
{
    auto it = m_worlds.find(worldID);
    if(it != m_worlds.end()) {
        return it->second;
    }

    return nullptr;
}

World* WorldManager::GetWorldByName(const string &worldName)
{
    string searchName = ToLower(worldName);
    for(auto& [_, pWorld] : m_worlds) {
        if(ToLower(pWorld->GetWorlName()) == searchName) {
            return pWorld;
        }
    }

    return nullptr;
}

void WorldManager::RegisterEvents()
{
    RegisterPacketEvent<ItemActivateRequest>(NET_GAME_PACKET_ITEM_ACTIVATE_REQUEST);
    RegisterPacketEvent<PlayerCollect>(NET_GAME_PACKET_ITEM_ACTIVATE_OBJECT_REQUEST);
    RegisterPacketEvent<UseDoorRequest>(NET_GAME_PACKET_USE_DOOR);
    RegisterPacketEvent<TileChangeRequest>(NET_GAME_PACKET_TILE_CHANGE_REQUEST);
    RegisterPacketEvent<State>(NET_GAME_PACKET_STATE);
    RegisterPacketEvent<PlayerSendIconState>(NET_GAME_PACKET_SET_ICON_STATE);
    RegisterPacketEvent<BlockActivate>(NET_GAME_PACKET_TILE_ACTIVATE_REQUEST);
    RegisterPacketEvent<PlayerPunched>(NET_GAME_PACKET_GOT_PUNCHED);
}

void WorldManager::OnHandleGamePacket(ENetEvent& event)
{
    if(!event.packet || event.packet->dataLength < (sizeof(GameUpdatePacket) + sizeof(uint32))) {
        return;
    }

    GameUpdatePacket* pGamePacket = GetGamePacketFromEnetPacket(event.packet);
    if(!pGamePacket) {
        return;
    }

    GamePlayer* pPlayer = (GamePlayer*)event.peer->data;
    if(event.peer != pPlayer->GetPeer()) {
        return;
    }

    if(!pPlayer->HasState(PLAYER_STATE_IN_GAME)) {
        return;
    }

    if(!pPlayer->CanProcessGamePacket((eGamePacketType)pGamePacket->type)) {
        pPlayer->SendFakePingReply();
        return;
    }

    World* pWorld = GetWorldByID(pPlayer->GetCurrentWorld());
    if(!pWorld) {
        return;
    }

    if(pGamePacket->type != NET_GAME_PACKET_NPC) {
        pPlayer->GetLastActionTime().Reset();
    }

    m_packetEvents.Dispatch((eGamePacketType)pGamePacket->type, pPlayer, pWorld, pGamePacket);
}

void WorldManager::UpdateWorlds()
{
    if(m_lastWorldUpdateTime.GetElapsedTime() < TICK_INTERVAL) {
        return;
    }

    for(auto it = m_worlds.begin(); it != m_worlds.end(); ++it) {
        World* pWorld = it->second;

        if(!pWorld || pWorld->IsWaitingForClose()) {
            continue;
        }

        pWorld->UpdateSteamActivations();
        ChemsynthAlgorithm::UpdateWorldChemsynth(pWorld);
        
        if(pWorld->GetPlayerCount() == 0 && (pWorld->GetOfflineTime().GetElapsedTime() >= 10 * 60 * 1000)) {
            pWorld->SetWaitingForClose(true);
            SaveWorldToDatabase(pWorld, true);

        }
        else if(pWorld->GetLastSaveTime().GetElapsedTime() >= 15 * 60 * 1000) {
            SaveWorldToDatabase(pWorld, false);
            pWorld->GetLastSaveTime().Reset();
        }
    }

    m_lastWorldUpdateTime.Reset();
}

/**
 * currently no fallback if failed to save
 * we just dont care if its saved rn lol
 */
void WorldManager::SaveWorldToDatabase(World* pWorld, bool closeWorld)
{
    if(!pWorld) {
        return;
    }

    File file;
    string worldSavePath = GetContext()->GetGameConfig()->worldSavePath + "/world_" + ToString(pWorld->GetID()) + ".bin";
    if(!file.Open(worldSavePath, FILE_MODE_WRITE)) {
        return;
    }

    MemoryBuffer memSize;
    pWorld->Serialize(memSize, true, true);
    
    uint32 worldMemSize = memSize.GetOffset();
    uint8* pWorldData = new uint8[worldMemSize];

    MemoryBuffer memBuffer(pWorldData, worldMemSize);
    pWorld->Serialize(memBuffer, true, true);

    if(file.Write(pWorldData, worldMemSize) != worldMemSize) {
        SAFE_DELETE_ARRAY(pWorldData);
        return;
    }

    file.Close();
    SAFE_DELETE_ARRAY(pWorldData);

    QueryRequest req = MakeSaveWorld(pWorld->GetWorlName(), pWorld->GetID(), NET_ID_WORLD_MANAGER);
    DatabaseWorldExec(GetContext()->GetDatabasePool(), DB_WORLD_SAVE, req);

    if(closeWorld) {
        /**
         * send master to delete session
         */
    }
}

void WorldManager::ForceSaveAllWorlds()
{
    for(auto& [_, pWorld] : m_worlds) {
        if(!pWorld) {
            continue;
        }

        SaveWorldToDatabase(pWorld, true);
    }
}

WorldManager* GetWorldManager() { return WorldManager::GetInstance(); }