#pragma once

#include "Player/Player.h"
#include "Player/Role.h"

enum ePlayerState
{
    PLAYER_STATE_LOGIN_REQUEST = 1 << 0,
    PLAYER_STATE_CHECKING_SESSION = 1 << 1,
    PLAYER_STATE_LOGIN_GETTING_ACCOUNT = 1 << 2,
    PLAYER_STATE_LOADING_ACCOUNT = 1 << 3,
    PLAYER_STATE_ENTERING_GAME = 1 << 4,
    PLAYER_STATE_IN_GAME = 1 << 5
};

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

    void SaveToDatabase();

    void SetJoinWorld(bool joining) { m_joiningWorld = joining; }
    bool IsJoiningWorld() { return m_joiningWorld; }
    
    void SetCurrentWorld(uint32 worldID) { m_currentWorldID = worldID; }
    uint32 GetCurrentWorld() const { return m_currentWorldID; }

    string GetDisplayName();
    string GetSpawnData(bool local);

    void SetWorldPos(const Vector2Int& pos) { m_worldPos = pos; }
    Vector2Int GetWorldPos() const { return m_worldPos; }

    Role* GetRole() const { return m_pRole; };

private:
    uint32 m_state;
    bool m_joiningWorld;
    uint32 m_currentWorldID;

    Vector2Int m_worldPos;
    uint32 m_guestID;

    Role* m_pRole;
};