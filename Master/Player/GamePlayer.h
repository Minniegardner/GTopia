#pragma once

#include "Player/Player.h"
#include "Packet/PacketUtils.h"

enum ePlayerState
{
    PLAYER_STATE_UNKNOWN,
    PLAYER_STATE_LOGIN_REQUEST,
    PLAYER_STATE_LOGIN_GETTING_ACCOUNT,
    PLAYER_STATE_LOGIN_CHECKING_ACCOUNT,
    PLAYER_STATE_COUNT_CREATED_FROM_IP,
    PLAYER_STATE_CREATING_ACCOUNT,
    PLAYER_STATE_UPDATE_IDENTIFIERS,
    PLAYER_STATE_SWITCHING_TO_GAME
};

class GamePlayer : public Player {
public:
    GamePlayer(ENetPeer* pPeer);
    ~GamePlayer();

public:
    void SetState(ePlayerState state) { m_state = state; }
    ePlayerState GetState() const { return m_state; }

    void OnHandleDatabase(QueryTaskResult&& result) override;

    void StartLoginRequest(ParsedTextPacket<25>& packet);
    void LoginGetAccount();
    void LoginCheckingAccount(QueryTaskResult&& result);
    void CheckCountOfCreatedAccsFromIP(QueryTaskResult&& result);
    void CreatingAccount(QueryTaskResult&& result);
    void UpdatedIdentifiers(QueryTaskResult&& result);
    void SwitchingToGame();

private:
    ePlayerState m_state;
    bool m_triedHashFallback = false;
};