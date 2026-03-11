#include "World.h"
#include "IO/Log.h"
#include "../Player/GamePlayer.h"
#include "Packet/NetPacket.h"

#include "IO/File.h"

World::World()
: m_worldID(0)
{
}

World::~World()
{
}

bool World::PlayerJoinWorld(GamePlayer* pPlayer)
{
    if(!pPlayer) {
        return false;
    }

    pPlayer->SetJoinWorld(false);
    pPlayer->SetCurrentWorld(m_worldID);
    m_players.push_back(pPlayer);

    TileInfo* pMainDorrTile = GetTileManager()->GetMainDoorTile();
    if(!pMainDorrTile) {
        pPlayer->SetWorldPos({ 0, 0 });
    }
    else {
        pPlayer->SetWorldPos(pMainDorrTile->GetPos() * 32);
    }

    MemoryBuffer memSize;
    Serialize(memSize, true, false);
    
    uint32 worldMemSize = memSize.GetOffset();
    uint8* pWorldData = new uint8[worldMemSize];

    MemoryBuffer memBuffer(pWorldData, worldMemSize);
    Serialize(memBuffer, true, false);

    GameUpdatePacket packet;
    packet.type = NET_GAME_PACKET_SEND_MAP_DATA;
    packet.netID = -1;
    packet.flags |= NET_GAME_PACKET_FLAGS_EXTENDED;
    packet.extraDataSize = worldMemSize;
    SendENetPacketRaw(NET_MESSAGE_GAME_PACKET, &packet, sizeof(GameUpdatePacket), pWorldData, pPlayer->GetPeer());
    SAFE_DELETE_ARRAY(pWorldData);

    pPlayer->SendOnSpawn(pPlayer->GetSpawnData(true));
    return true;
}

void World::PlayerLeaverWorld(GamePlayer* pPlayer)
{
    for(uint16 i = 0; i < m_players.size(); ++i) {
        if(m_players[i] == pPlayer) {
            m_players[i] = m_players.back();
            m_players.pop_back();
        }
    }

    pPlayer->SetCurrentWorld(0);
}

void World::SendToAll(const std::function<void(GamePlayer *)>& func)
{
    for(auto& pPlayer : m_players) {
        func(pPlayer);
    }
}
