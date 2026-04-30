#pragma once

#include "Precompiled.h"
#include "Utils/Timer.h"

class GamePlayer;

class PlayerManager {
public:
    PlayerManager();
    ~PlayerManager();

public:
    static PlayerManager* GetInstance()
    {
        static PlayerManager instance;
        return &instance;
    }

    GamePlayer* IsPlayerAlreadyOn(GamePlayer* pNewPlayer);
    GamePlayer* GetPlayerByNetID(uint32 netID);
    GamePlayer* GetPlayerByUserID(uint32 userID);
    GamePlayer* GetPlayerByRawName(const string& rawName);
    std::vector<GamePlayer*> FindPlayersByNamePrefix(const string& query, bool sameWorldOnly, uint32 worldID);
    void AddPlayer(GamePlayer* pPlayer);
    void RemovePlayer(uint32 netID);
    void RemoveAllPlayers();
    uint32 GetPlayerCount();
    uint32 GetOnlineCount() { return GetPlayerCount(); }

    void SetTotalPlayerCount(uint32 totalPlayerCount) { m_totalPlayerCount = totalPlayerCount; }
    uint32 GetTotalPlayerCount();

    void UpdatePlayers();
    void SaveAllToDatabase();

private:
    std::unordered_map<uint32, GamePlayer*> m_gamePlayers;
    std::vector<GamePlayer*> m_pendingDelete;
    Timer m_lastUpdateTime;

    uint32 m_totalPlayerCount;
};

PlayerManager* GetPlayerManager();