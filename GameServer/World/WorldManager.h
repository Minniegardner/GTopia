#pragma once

#include "Precompiled.h"
#include "World/WorldManagerBase.h"
#include "../Player/GamePlayer.h"
#include "World.h"
#include "Packet/NetPacket.h"
#include "Event/EventDispatcher.h"

class WorldManager : public WorldManagerBase {
public:
    WorldManager();
    ~WorldManager();

public:
    static WorldManager* GetInstance()
    {
        static WorldManager instance;
        return &instance;
    }

public:
    void OnHandleDatabase(QueryTaskResult&& result) override;
    void OnHandleTCP(VariantVector&& result) override;

public:
    void PlayerJoinRequest(GamePlayer* pPlayer, const string& worldName);
    void PlayerLeaveWorld(GamePlayer* pPlayer);

    void Kill();

    World* GetWorldByID(uint32 worldID);
    World* GetWorldByName(const string& worldName);

    void RegisterEvents();
    void OnHandleGamePacket(ENetEvent& event);

    void UpdateWorlds();
    void SaveWorldToDatabase(World* pWorld, bool closeWorld);
    uint32 RepairLegacyMagplantWorlds(bool createBackup = true, bool dryRun = false);

    void ForceSaveAllWorlds();

    uint32 GetWorldCount() { return m_worlds.size(); }

private:
    template<class T>
    void RegisterPacketEvent(eGamePacketType type)
    {
        m_packetEvents.Register(
            type,
            Delegate<GamePlayer*, World*, GameUpdatePacket*>::Create<&T::Execute>()
        );
    }

private:
    Timer m_lastWorldUpdateTime;

    std::unordered_map<uint32, World*> m_worlds;
    EventDispatcher<eGamePacketType, GamePlayer*, World*, GameUpdatePacket*> m_packetEvents;
};

WorldManager* GetWorldManager();