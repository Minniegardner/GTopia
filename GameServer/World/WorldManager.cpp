#include "WorldManager.h"
#include "Algorithm/ChemsynthAlgorithm.h"
#include "Algorithm/GhostAlgorithm.h"
#include "../Context.h"
#include "../Server/MasterBroadway.h"
#include "../Server/GameServer.h"
#include "Database/Table/WorldDBTable.h"
#include "IO/File.h"

#include <filesystem>

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
            if(result.size() < 3) {
                return;
            }

            int32 oprResult = result[1].GetINT();
            int32 playerNetID = result[2].GetINT();

            GamePlayer* pPlayer = GetGameServer()->GetPlayerByNetID(playerNetID);
            if(!pPlayer) {
                return;
            }

            if(oprResult != TCP_RESULT_OK) {
                pPlayer->SendOnFailedToEnterWorld();
                pPlayer->SendOnConsoleMessage("Unable to move you to this world, please try again later");
                return;
            }
            
            uint32 serverID = result[3].GetUINT();
            uint32 worldID = result[4].GetUINT();

            if(serverID == GetContext()->GetID()) {
                World* pWorld = GetWorldByID(worldID);
                if(!pWorld || !pWorld->PlayerJoinWorld(pPlayer)) {
                    pPlayer->SetJoinWorld(false);
                    pPlayer->SendOnFailedToEnterWorld();
                    pPlayer->SendOnConsoleMessage("Unable to join world");
                }
            }
            else {
                if(result.size() < 10) {
                    pPlayer->SetJoinWorld(false);
                    pPlayer->SendOnFailedToEnterWorld();
                    pPlayer->SendOnConsoleMessage("Unable to redirect to target subserver");
                    return;
                }

                const string serverIP = result[5].GetString();
                const uint16 serverPort = (uint16)result[6].GetUINT();
                string worldName = ToUpper(result[7].GetString());
                const uint32 userID = result[8].GetUINT();
                const uint32 loginToken = result[9].GetUINT();

                if(userID != pPlayer->GetUserID() || serverIP.empty() || serverPort == 0 || worldName.empty() || loginToken == 0) {
                    pPlayer->SetJoinWorld(false);
                    pPlayer->SendOnFailedToEnterWorld();
                    pPlayer->SendOnConsoleMessage("Unable to redirect to target subserver");
                    return;
                }

                string doorID = ToUpper(pPlayer->ConsumePendingDoorWarpID());
                string redirectDoor = worldName;
                if(!doorID.empty()) {
                    redirectDoor += ":" + doorID;
                }

                PlayerLeaveWorld(pPlayer);
                pPlayer->SetJoinWorld(false);
                pPlayer->SetSwitchingSubserver(true);
                pPlayer->SendOnSendToServer(serverPort, loginToken, userID, serverIP, redirectDoor, "-1", LOGIN_MODE_REDIRECT_SUBSERVER_SILENT);
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

    pPlayer->SetJoinWorld(true);
    World* pWorld = GetWorldByName(worldName);

    if(pWorld) {
        if(!pWorld->PlayerJoinWorld(pPlayer)) {
            pPlayer->SendOnFailedToEnterWorld();
        }

        pPlayer->SetJoinWorld(false);
        return;
    }

    GetMasterBroadway()->SendPlayerWorldJoin(pPlayer->GetNetID(), pPlayer->GetUserID(), worldName);
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
    ForceSaveAllWorlds();

    for(auto& [_, pWorld] : m_worlds) {
        if(pWorld) {
            GhostAlgorithm::DestroyWorldState(pWorld->GetID());
        }
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
        GhostAlgorithm::UpdateWorldGhosts(pWorld);
        
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

uint32 WorldManager::RepairLegacyMagplantWorlds(bool createBackup, bool dryRun)
{
    const string worldSavePath = GetContext()->GetGameConfig()->worldSavePath;
    if(worldSavePath.empty()) {
        return 0;
    }

    std::error_code ec;
    if(!std::filesystem::exists(worldSavePath, ec) || !std::filesystem::is_directory(worldSavePath, ec)) {
        return 0;
    }

    uint32 repairedFiles = 0;
    for(const auto& entry : std::filesystem::directory_iterator(worldSavePath, ec)) {
        if(ec || !entry.is_regular_file()) {
            continue;
        }

        const std::filesystem::path filePath = entry.path();
        const string fileName = filePath.filename().string();
        if(fileName.rfind("world_", 0) != 0 || filePath.extension().string() != ".bin") {
            continue;
        }

        File inFile;
        if(!inFile.Open(filePath.string())) {
            continue;
        }

        const uint32 fileSize = inFile.GetSize();
        if(fileSize == 0) {
            inFile.Close();
            continue;
        }

        std::vector<uint8> inputData(fileSize);
        if(inFile.Read(inputData.data(), fileSize) != fileSize) {
            inFile.Close();
            continue;
        }
        inFile.Close();

        MemoryBuffer readBuffer(inputData.data(), fileSize);
        World repairWorld;
        if(!repairWorld.Serialize(readBuffer, false, true)) {
            continue;
        }

        MemoryBuffer sizeBuffer;
        if(!repairWorld.Serialize(sizeBuffer, true, true)) {
            continue;
        }

        const uint32 outSize = sizeBuffer.GetOffset();
        std::vector<uint8> outputData(outSize);
        MemoryBuffer writeBuffer(outputData.data(), outSize);
        if(!repairWorld.Serialize(writeBuffer, true, true)) {
            continue;
        }

        if(inputData.size() == outputData.size() && memcmp(inputData.data(), outputData.data(), inputData.size()) == 0) {
            continue;
        }

        repairedFiles++;
        if(dryRun) {
            continue;
        }

        if(createBackup) {
            const std::filesystem::path backupPath = filePath.string() + ".bak_magplantfix";
            if(!std::filesystem::exists(backupPath, ec)) {
                std::filesystem::copy_file(filePath, backupPath, std::filesystem::copy_options::overwrite_existing, ec);
            }
        }

        File outFile;
        if(!outFile.Open(filePath.string(), FILE_MODE_WRITE)) {
            continue;
        }

        outFile.Write(outputData.data(), outSize);
        outFile.Close();
    }

    return repairedFiles;
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