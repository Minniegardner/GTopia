#pragma once

#include "Precompiled.h"
#include "World/WorldInfo.h"
#include "Packet/GamePacket.h"

#include <deque>
#include <chrono>

class GamePlayer;

struct WorldBanContext {
    uint32 UserID = 0;
    uint32 AdminUserID = 0;
    std::chrono::hours Duration = std::chrono::hours(1);  // Default 1 hour
    std::chrono::steady_clock::time_point BannedAt = std::chrono::steady_clock::now();
};

class World : public WorldInfo {
public:
    World();
    ~World();

public:
    void SetID(uint32 id) { m_worldID = id; }
    uint32 GetID() const { return m_worldID; }

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
    GamePlayer* GetPlayerOnTile(const Vector2Int& tilePos);
    GamePlayer* GetPlayerByNetID(uint32 netID);

    void OnAddLock(GamePlayer* pPlayer, TileInfo* pTile, uint16 lockID);
    void OnRemoveLock(GamePlayer* pPlayer, TileInfo* pTile);

    bool IsPlayerWorldOwner(GamePlayer* pPlayer);
    bool IsPlayerWorldAdmin(GamePlayer* pPlayer);
    bool CanPull(GamePlayer* pTarget, GamePlayer* pInvoker);
    bool CanKick(GamePlayer* pTarget, GamePlayer* pInvoker);
    bool CanBan(GamePlayer* pTarget, GamePlayer* pInvoker);
    void PullPlayer(GamePlayer* pTarget, GamePlayer* pInvoker);
    void KickPlayer(GamePlayer* pTarget, GamePlayer* pInvoker);
    void ForceLeavePlayer(GamePlayer* pTarget);

    bool CanPlace(GamePlayer* pPlayer, TileInfo* pTile);
    bool CanBreak(GamePlayer* pPlayer, TileInfo* pTile);

    void DropObject(TileInfo* pTile, WorldObject& obj, bool merge);
    bool SuckerCheck(WorldObject& obj);

    void DropObject(const WorldObject& obj);
    void RemoveObject(uint32 objectID);
    void ModifyObject(const WorldObject& obj);

    void TriggerSteamPulse(TileInfo* pTile);
    void QueueSteamActivation(TileInfo* pTile, uint32 delayMS);
    void UpdateSteamActivations();
    void SendSteamPacket(uint8 mode, const Vector2Int& tilePos);

    void SendCurrentWeatherToAll();
    uint32 GetPlayerCount() const { return m_players.size(); }
    const std::vector<GamePlayer*>& GetPlayers() const { return m_players; }
    Timer& GetLastSaveTime() { return m_worldLastSaveTime; }
    Timer& GetOfflineTime() { return m_worldOfflineTime; }

    void SetWaitingForClose(bool waiting) { m_waitingForClose = waiting; }
    bool IsWaitingForClose() const { return m_waitingForClose; }
    uint64 GetLastKickAllMS() const { return m_lastKickAllMS; }
    void SetLastKickAllMS(uint64 value) { m_lastKickAllMS = value; }

    // Ban system methods (1:1 from GrowtopiaV1)
    void AddBannedPlayer(const WorldBanContext& banCtx);
    bool IsPlayerBanned(uint32 userID);
    void RemoveBannedPlayer(uint32 userID);
    void ClearBannedPlayers();
    void BanPlayer(GamePlayer* pTarget, GamePlayer* pInvoker, uint32 banDurationHours = 1);

private:
    uint32 m_worldID;
    std::vector<GamePlayer*> m_players;
    std::unordered_map<uint32, WorldBanContext> m_bannedPlayers;  // Per-world ban list

    Timer m_worldOfflineTime;
    Timer m_worldLastSaveTime;

    struct SteamActivationEntry {
        Vector2Int tilePos;
        uint64 activateAtMS = 0;
    };

    std::deque<SteamActivationEntry> m_steamActivationQueue;
    uint64 m_lastSteamActivationTick = 0;
    uint64 m_lastKickAllMS = 0;

    bool m_waitingForClose;
};