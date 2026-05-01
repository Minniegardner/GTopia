#pragma once

#include "Precompiled.h"
#include "World/WorldInfo.h"
#include "Packet/GamePacket.h"
#include <chrono>
#include <unordered_map>
#include <functional>

class GamePlayer;

class World : public WorldInfo {
public:
    World();
    ~World();

public:
    void SetID(uint32 id) { m_worldID = id; }
    uint32 GetID() const { return m_worldID; }

    void SaveToDatabase();
    void Update();

    bool PlayerJoinWorld(GamePlayer* pPlayer);
    void PlayerLeaverWorld(GamePlayer* pPlayer);

    void SendSkinColorUpdateToAll(GamePlayer* pPlayer);
    void SendTalkBubbleAndConsoleToAll(const string& message, bool stackBubble, GamePlayer* pPlayer = nullptr);
    void SendConsoleMessageToAll(const string& message);
    void SendNameChangeToAll(GamePlayer* pPlayer);
    void SendSetCharPacketToAll(GamePlayer* pPlayer);
    void SendClothUpdateToAll(GamePlayer* pPlayer);
    void SendParticleEffectToAll(float coordX, float coordY, uint32 particleType, float particleSize = 0, int32 delay = -1);
    void SendTileUpdate(TileInfo* pTile);
    void SendTileUpdate(uint16 tileX, uint16 tileY);
    void SendTileUpdateMultiple(const std::vector<TileInfo*>& tiles);
    void SendTileApplyDamage(uint16 tileX, uint16 tileY, int32 damage, int32 netID);
    void SendLockPacketToAll(int32 userID, int32 lockID, std::vector<TileInfo*>& tiles, TileInfo* pLockTile);
    void PlaySFXForEveryone(const string& fileName, int32 delay = -1);

    void SendGamePacketToAll(GameUpdatePacket* pPacket, GamePlayer* pExceptMe = nullptr, uint8* pExtraData = nullptr);
    void HandleTilePackets(GameUpdatePacket* pGamePacket);

    bool PlayerHasAccessOnTile(GamePlayer* pPlayer, TileInfo* pTile);

    void OnAddLock(GamePlayer* pPlayer, TileInfo* pTile, uint16 lockID);
    void OnRemoveLock(GamePlayer* pPlayer, TileInfo* pTile);

    bool IsPlayerWorldOwner(GamePlayer* pPlayer);
    bool IsPlayerWorldAdmin(GamePlayer* pPlayer);

    void DropObject(TileInfo* pTile, WorldObject& obj, bool merge);

    void DropObject(const WorldObject& obj);
    void RemoveObject(uint32 objectID);
    void ModifyObject(const WorldObject& obj);

    void SendCurrentWeatherToAll();
    uint32 GetPlayerCount() const { return m_players.size(); }
    Timer& GetLastSaveTime() { return m_worldLastSaveTime; }
    Timer& GetOfflineTime() { return m_worldOfflineTime; }

    void SetDeleteFlag(bool bDelete) { m_deleteFlag = bDelete; }
    bool HasDeleteFlag() const { return m_deleteFlag; }

private:
    uint32 m_worldID;
    std::vector<GamePlayer*> m_players;

    Timer m_worldOfflineTime;
    Timer m_worldLastSaveTime;

    bool m_deleteFlag;
    
public:
    struct Npc {
        uint16 Type = 0;
        float PosX = 0.0f;
        float PosY = 0.0f;

        float LastPosX = 0.0f;
        float LastPosY = 0.0f;
        float LastSpeed = 0.0f;
        std::chrono::steady_clock::time_point LastMove = std::chrono::steady_clock::now();
    };

private:
    uint32 m_NpcID = 0;
    std::unordered_map<uint32, Npc> m_Npcs;

public:
    uint32 AddNpc(const Npc& npc);
    void RemoveNpc(uint32 npcID);
    void TeleportNpc(uint32 npcID, float x, float y);
    void MoveNpc(uint32 npcID, float speed, float x, float y);
    void Npcs(const std::function<void(uint32, uint16, Npc*)>& fn);
    std::pair<float, float> GetNpcLerpPosition(uint32 npcID);
    bool IsGhost(uint16 type);
    uint32 GetNpcID() const { return m_NpcID; }
    uint32 IncrementNpcID() { return ++m_NpcID; }
    void SetNpcID(uint32 val) { m_NpcID = val; }
    size_t GetNpcCount() const { return m_Npcs.size(); }
};