#pragma once

#include "Player/Player.h"
#include "Player/Role.h"
#include "Utils/Timer.h"
#include "Player/PlayMod.h"

enum ePlayerState
{
    PLAYER_STATE_LOGIN_REQUEST = 1 << 0,
    PLAYER_STATE_CHECKING_SESSION = 1 << 1,
    PLAYER_STATE_LOGIN_GETTING_ACCOUNT = 1 << 2,
    PLAYER_STATE_LOADING_ACCOUNT = 1 << 3,
    PLAYER_STATE_ENTERING_GAME = 1 << 4,
    PLAYER_STATE_IN_GAME = 1 << 5,
    PLAYER_STATE_RENDERING_WORLD = 1 << 6,
    PLAYER_STATE_CREATING_GROWID = 1 << 7
};

enum ePlayerSubState
{
    PLAYER_SUB_GROWID_CHECK_IDENTIFIERS,
    PLAYER_SUB_GROWID_CHECK_NAME,
    PLAYER_SUB_GROWID_SUCCESS
};

class TileInfo;

class GamePlayer : public Player {
public:
    GamePlayer(ENetPeer* pPeer);
    ~GamePlayer();

public:
    void SetState(ePlayerState state) { m_state |= (uint32)state; }
    void RemoveState(ePlayerState state) { m_state &= ~(uint32)state; }
    uint32 HasState(ePlayerState state) { return m_state & state; }

    void OnHandleDatabase(QueryTaskResult&& result) override;
    void OnHandleTCP(VariantVector&& result) override;

    void StartLoginRequest(ParsedTextPacket<25>& packet);
    void CheckingLoginSession(VariantVector&& result);
    void LoadingAccount(QueryTaskResult&& result);
    void TransferingPlayerToGame();

    void HandleRenderWorld(VariantVector&& result);
    void HandleCreateGrowID(QueryTaskResult&& result);

    void SaveToDatabase();
    void LogOff();

    void Update();

    void SetJoinWorld(bool joining) { m_joiningWorld = joining; }
    bool IsJoiningWorld() { return m_joiningWorld; }
    
    void SetCurrentWorld(uint32 worldID) { m_currentWorldID = worldID; }
    uint32 GetCurrentWorld() const { return m_currentWorldID; }

    string GetDisplayName();
    string GetRawName();
    string GetSpawnData(bool local);

    void SetWorldPos(float x, float y) { m_worldPos.x = x; m_worldPos.y = y; }
    Vector2Float GetWorldPos() const { return m_worldPos; }

    void SetRespawnPos(float x, float y) { m_respawnPos.x = x; m_respawnPos.y = y; }
    Vector2Float GetRespawnPos() const { return m_respawnPos; }

    Role* GetRole() const { return m_pRole; };
    void ToggleCloth(uint16 itemID);

    void ModifyInventoryItem(uint16 itemID, int16 amount);
    void TrashItem(uint16 itemID, uint8 amount);

    void AddPlayMod(ePlayModType modType, bool silent = false);
    void RemovePlayMod(ePlayModType modType, bool silent = false);
    void UpdateNeededPlayModThings();
    void UpdatePlayMods();
    void UpdateTorchPlayMod();

    bool CanActivateItemNow() { return Time::GetSystemTime() - m_lastItemActivateTime >= 100; };
    void ResetItemActiveTime() { m_lastItemActivateTime = Time::GetSystemTime(); }

    bool HasGrowID() { return !m_loginDetail.tankIDPass.empty(); }
    void ExecGrowIDIdentifierCheck(bool fromDialog, const VariantVector& extraData = VariantVector{});

    void SendPositionToWorldPlayers();

    Timer& GetLastActionTime() { return m_lastActionTime; }
    Timer& GetLastDBSaveTime() { return m_lastDbSaveTime; }

    bool IsLoggingOff() const { return m_loggingOff; }

private:
    uint32 m_state;
    bool m_joiningWorld;
    uint32 m_currentWorldID;

    uint64 m_lastItemActivateTime;
    Timer m_lastActionTime;

    uint32 m_guestID;

    bool m_loggingOff;

    Vector2Float m_respawnPos;
    Vector2Float m_worldPos;

    Timer m_lastDbSaveTime;

    Role* m_pRole;
};