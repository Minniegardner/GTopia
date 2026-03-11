#pragma once

#include "Precompiled.h"
#include "World/WorldInfo.h"
#include <functional>

class GamePlayer;

class World : public WorldInfo {
public:
    World();
    ~World();

public:
    void SetID(uint32 id) { m_worldID = id; }
    uint32 GetID() const { return m_worldID; }

    bool PlayerJoinWorld(GamePlayer* pPlayer);
    void PlayerLeaverWorld(GamePlayer* pPlayer);

    void SendToAll(const std::function<void(GamePlayer*)>& func);

private:
    uint32 m_worldID;
    std::vector<GamePlayer*> m_players;
};