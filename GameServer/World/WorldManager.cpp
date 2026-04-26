#include "WorldManager.h"
#include "../Context.h"
#include "../Server/MasterBroadway.h"
#include "../Server/GameServer.h"
#include "Database/Table/WorldDBTable.h"
#include "../Player/PlayerManager.h"
#include "IO/File.h"

#include "../Event/UDP/GamePacket/ItemActivateRequest.h"
#include "../Event/UDP/GamePacket/TileChangeRequest.h"
#include "../Event/UDP/GamePacket/State.h"

WorldManager::WorldManager()
{
    RegisterEvents();
}

WorldManager::~WorldManager()
{
    Kill();
}

void WorldManager::HandleWorldInit(VariantVector&& result)
{
    if(result.size() < 2) {
        return;
    }

    QueryRequest req = WorldDB::GetWorldData(result[1].GetString());
    req.callback = &WorldManager::WorldDBInitCB;

    DatabaseWorldExec(GetContext()->GetDatabasePool(), req);   
}

void WorldManager::WorldDBInitCB(QueryTaskResult&& result)
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
            return;
        }

        uint32 fileSize = file.GetSize();
        uint8* pData = new uint8[fileSize];

        if(file.Read(pData, fileSize) != fileSize) {
            pMasterBroadway->SendWorldInitResult(false, worldID);
            file.Close();
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

    pWorld->SetID(worldID);
    GetWorldManager()->AddWorld(pWorld);

    pMasterBroadway->SendWorldInitResult(true, worldID);
}

void WorldManager::HandlePlayerJoin(VariantVector&& result)
{
    int32 oprResult = result[1].GetINT();
    uint32 playerUserID = result[2].GetUINT();

    GamePlayer* pPlayer = GetPlayerManager()->GetPlayerByUserID(playerUserID);
    if(!pPlayer) {
        return;
    }

    if(oprResult != TCP_RESULT_OK) {
        pPlayer->SendOnFailedToEnterWorld();
        pPlayer->SendOnConsoleMessage("Unable to join to this world, please try again later.");
        return;
    }
    
    uint32 serverID = result[3].GetUINT();
    uint32 worldID = result[4].GetUINT();

    World* pWorld = GetWorldByID(worldID);
    if(!pWorld || serverID != GetContext()->GetID()) {
        string serverIP = result[5].GetString();
        uint16 serverPort = result[6].GetUINT();

        PlayerLoginDetail& loginDetail = pPlayer->GetLoginDetail();
        pPlayer->SendOnSendToServer(serverPort, loginDetail.token, loginDetail.user, serverIP, loginDetail.loginMode);
        return;
    }

    if(serverID == GetContext()->GetID()) {
        if(!pWorld->PlayerJoinWorld(pPlayer)) {
            pPlayer->SendOnFailedToEnterWorld();
            pPlayer->SendOnConsoleMessage("Failed to join world?! please try again later.");
        }
    }
}

void WorldManager::SendWorldSelectMenu(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return;
    }

    pPlayer->SendOnRequestWorldSelectMenu("");
}

void WorldManager::PlayerJoinRequest(GamePlayer* pPlayer, const string& worldName)
{
    if(!pPlayer || pPlayer->IsJoiningWorld()) {
        return;
    }

    string targetWorldName = ToUpper(worldName);
    RemoveGTColorCodes(targetWorldName);

    if(targetWorldName.empty() || targetWorldName.size() > 24) {
        pPlayer->SendOnConsoleMessage("Sorry, world name length must between 1 and 24.");
        pPlayer->SendOnFailedToEnterWorld();
        return;
    }

    if(!IsValidWorldName(targetWorldName)) {
        pPlayer->SendOnConsoleMessage("Sorry, spaces and special characters are not allowed in world or door names. Try again.");
        pPlayer->SendOnFailedToEnterWorld();
        return;
    }

    if(targetWorldName == "EXIT") {
        pPlayer->SendOnConsoleMessage("Exit from what? Click back if you're done playing.");
        pPlayer->SendOnFailedToEnterWorld();
        return;
    }

    pPlayer->SetJoiningWorld(true);
    World* pWorld = GetWorldByName(worldName);
    if(pWorld) {
        if(pWorld->HasDeleteFlag()) {
            pPlayer->SendOnFailedToEnterWorld();
            pPlayer->SendOnConsoleMessage("Unable to move you to this world, please try again in a few seconds.");
            return;
        }

        /**
         * we shouldnt handle max player count in here
         * we should ask master for target world player count first
         * otherwise if we do it like that players will flood in
         * but yea, just leave it for now
         */
        uint32 worldMaxPlayerCount = GetContext()->GetGameConfig()->worldMaxPlayerCount;
        if(pWorld->GetPlayerCount() >= worldMaxPlayerCount) {
            pPlayer->SendOnConsoleMessage("Oops, `5" + targetWorldName + "`` already has `4" + ToString(worldMaxPlayerCount) + "`` people in it. Try again later.");
            pPlayer->SendOnFailedToEnterWorld();
            return;
        }

        if(!pWorld->PlayerJoinWorld(pPlayer)) {
            pPlayer->SendOnConsoleMessage("Failed to join world?! please try again later.");
            pPlayer->SendOnFailedToEnterWorld();
        }

        pPlayer->SetJoiningWorld(false);
    }
    else {
        GetMasterBroadway()->SendPlayerWorldJoin(pPlayer->GetUserID(), worldName);
    }
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

    m_worlds.clear();
}

World* WorldManager::GetWorldByID(uint32 worldID)
{
    auto it = m_worlds.find(worldID);
    if(it != m_worlds.end()) {
        return it->second;
    }

    return nullptr;
}

World* WorldManager::GetWorldByName(const string& worldName)
{
    string searchName = ToLower(worldName);
    for(auto& [_, pWorld] : m_worlds) {
        if(ToLower(pWorld->GetWorlName()) == searchName) {
            return pWorld;
        }
    }

    return nullptr;
}

void WorldManager::AddWorld(World* pWorld)
{
    if(!pWorld) {
        return;
    }

    m_worlds.insert_or_assign(pWorld->GetID(), pWorld);
}

void WorldManager::RegisterEvents()
{
    RegisterPacketEvent<ItemActivateRequest>(NET_GAME_PACKET_ITEM_ACTIVATE_REQUEST);
    RegisterPacketEvent<TileChangeRequest>(NET_GAME_PACKET_TILE_CHANGE_REQUEST);
    RegisterPacketEvent<State>(NET_GAME_PACKET_STATE);
}

void WorldManager::OnHandleGamePacket(ENetEvent& event)
{
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

    for(auto& [_, pWorld] : m_worlds) {
        if(!pWorld) {
            continue;
        }

        if(!pWorld->HasDeleteFlag()) {
            pWorld->Update();
        }

        if(pWorld->HasDeleteFlag()) {
            m_pendingDelete.push_back(pWorld);
            continue;
        }
    }

    for(auto& pWorld : m_pendingDelete) {
        if(!GetWorldByID(pWorld->GetID())) {
            continue;
        }

        m_worlds.erase(pWorld->GetID());
    }

    m_pendingDelete.clear();
    m_lastWorldUpdateTime.Reset();
}

void WorldManager::ForceSaveAllWorlds()
{
    for(auto& [_, pWorld] : m_worlds) {
        if(!pWorld) {
            continue;
        }

        pWorld->SaveToDatabase();
    }
}

WorldManager* GetWorldManager() { return WorldManager::GetInstance(); }