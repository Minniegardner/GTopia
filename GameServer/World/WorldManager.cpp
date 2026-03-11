#include "WorldManager.h"
#include "../Context.h"
#include "../Server/MasterBroadway.h"
#include "../Server/GameServer.h"
#include "Database/Table/WorldDBTable.h"
#include "IO/File.h"

WorldManager::WorldManager()
{
}

WorldManager::~WorldManager()
{
}

void WorldManager::OnHandleDatabase(QueryTaskResult&& result)
{
    if(!result.result) {
        return;
    }

    MasterBroadway* pMasterBroadway = GetMasterBroadway();
    uint32 worldID = result.result->GetField("ID", 0).GetUINT();

    VariantVector data(3);
    data[0] = TCP_PACKET_WORLD_INIT;
    data[1] = false;
    data[2] = worldID;

    string path = GetContext()->GetGameConfig()->worldSavePath
    + "/world_" + ToString(worldID) + ".bin";

    World* pWorld = nullptr;

    if(IsFileExists(path)) {
        File file;
        if(!file.Open(path)) {
            pMasterBroadway->SendPacketRaw(data);
            return;
        }

        uint32 fileSize = file.GetSize();
        uint8* pData = new uint8[fileSize];

        if(file.Read(pData, fileSize) != fileSize) {
            pMasterBroadway->SendPacketRaw(data);
            SAFE_DELETE_ARRAY(pData);
            file.Close();
            return;
        }

        MemoryBuffer memBuffer(pData, fileSize);

        pWorld = new World();
        pWorld->Serialize(memBuffer, false, true);
        SAFE_DELETE_ARRAY(pData);
        file.Close();
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
        }
    }

    pWorld->SetID(worldID);

    m_worlds.insert_or_assign(worldID, pWorld);

    data[1] = true;
    pMasterBroadway->SendPacketRaw(data);
}

void WorldManager::OnHandleTCP(VariantVector&& result)
{
    if(result.empty()) {
        return;
    }

    if(result[1].GetType() == VARIANT_TYPE_BOOL) {
        GamePlayer* pPlayer = GetGameServer()->GetPlayerByNetID(result[2].GetINT());
        if(!pPlayer) {
            return;
        }

        if(!result[1].GetBool()) {
            pPlayer->SendOnFailedToEnterWorld();
            pPlayer->SendOnConsoleMessage("Unable to move you to this world, please try again");
            return;
        }

        return;
    }
    else if(result[1].GetType() == VARIANT_TYPE_STRING) {
        QueryRequest req = MakeGetWorldData(result[1].GetString(), GetNetID());
        DatabaseWorldExec(GetContext()->GetDatabasePool(), DB_WORLD_GET_DATA, req);
        return;
    }

    if(result[1].GetUINT() == GetContext()->GetID()) {
        World* pWorld = GetWorldByID(result[3].GetUINT());
        if(!pWorld) {
            return;
        }

        GamePlayer* pPlayer = GetGameServer()->GetPlayerByNetID(result[2].GetINT());
        if(!pPlayer) {
            return;
        }

        if(!pWorld->PlayerJoinWorld(pPlayer)) {
            pPlayer->SendOnFailedToEnterWorld();
            pPlayer->SendOnConsoleMessage("Unable to join world");
        }
    }
    else {
        //pPlayer->SendOnSendToServer();
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

    VariantVector data(4);
    data[0] = TCP_PACKET_WORLD_INIT;
    data[1] = (uint32)GetContext()->GetID();
    data[2] = pPlayer->GetNetID();
    data[3] = ToUpper(worldName);

    GetMasterBroadway()->SendPacketRaw(data);
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

    World* pWorld = GetWorldByID(pPlayer->GetCurrentWorld());
    m_packetEvents.Dispatch((eGamePacketType)pGamePacket->type, pPlayer, pWorld, pGamePacket);
}

WorldManager* GetWorldManager() { return WorldManager::GetInstance(); }