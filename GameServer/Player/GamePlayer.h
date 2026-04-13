#pragma once

#include "Player/Player.h"
#include "Player/Role.h"
#include "Utils/Timer.h"
#include "Player/PlayMod.h"
#include "Packet/GamePacket.h"

#include <unordered_set>

struct TradeOffer
{
    uint16 ID = 0;
    uint8 Amount = 0;
};

enum ePlayerState
{
    PLAYER_STATE_LOGIN_REQUEST = 1 << 0,
    PLAYER_STATE_CHECKING_SESSION = 1 << 1,
    PLAYER_STATE_LOGIN_GETTING_ACCOUNT = 1 << 2,
    PLAYER_STATE_LOADING_ACCOUNT = 1 << 3,
    PLAYER_STATE_ENTERING_GAME = 1 << 4,
    PLAYER_STATE_IN_GAME = 1 << 5,
    PLAYER_STATE_RENDERING_WORLD = 1 << 6
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

    int32 GetGems() const { return m_gems; }
    void SetGems(int32 gems) { m_gems = std::max(0, gems); }
    bool TrySpendGems(int32 amount);
    void AddGems(int32 amount);
    void SendOnSetBux();

    bool IsTrading() const { return m_isTrading; }
    void SetTrading(bool trading) { m_isTrading = trading; }
    uint32 GetTradingWithUserID() const { return m_tradingWithUserID; }
    void SetTradingWithUserID(uint32 userID) { m_tradingWithUserID = userID; }
    bool IsTradeAccepted() const { return m_isTradeAccepted; }
    void SetTradeAccepted(bool accepted) { m_isTradeAccepted = accepted; }
    uint64 GetTradeAcceptedAt() const { return m_tradeAcceptedAt; }
    void SetTradeAcceptedAt(uint64 timeMS) { m_tradeAcceptedAt = timeMS; }
    bool IsTradeConfirmed() const { return m_isTradeConfirmed; }
    void SetTradeConfirmed(bool confirmed) { m_isTradeConfirmed = confirmed; }
    uint64 GetTradeConfirmedAt() const { return m_tradeConfirmedAt; }
    void SetTradeConfirmedAt(uint64 timeMS) { m_tradeConfirmedAt = timeMS; }
    uint64 GetLastTradedAt() const { return m_lastTradedAt; }
    void SetLastTradedAt(uint64 timeMS) { m_lastTradedAt = timeMS; }
    uint64 GetLastChangeTradeDeal() const { return m_lastChangeTradeDeal; }
    void SetLastChangeTradeDeal(uint64 timeMS) { m_lastChangeTradeDeal = timeMS; }
    uint64 GetLastAcceptedTrade() const { return m_lastAcceptedTrade; }
    void SetLastAcceptedTrade(uint64 timeMS) { m_lastAcceptedTrade = timeMS; }
    const std::vector<TradeOffer>& GetTradeOffers() const { return m_tradeOffers; }
    bool IsTradeOfferExists(uint16 itemID) const;
    void RemoveTradeOffer(uint16 itemID);
    void AddTradeOffer(TradeOffer tradeOffer);
    void ClearTradeOffers();
    void StartTrade(GamePlayer* player);
    void CancelTrade(bool busy = true, std::string customMessage = "");
    void ModifyOffer(uint16 itemID, uint16 amount = 0);
    void RemoveOffer(uint16 itemID);
    void AcceptOffer(bool status = true);
    void ConfirmOffer();
    void SendTradeStatus(GamePlayer* player);
    void SendTradeAlert(GamePlayer* player);
    void SendStartTrade(GamePlayer* player);
    void SendForceTradeEnd();
    void AddTradeHistory(std::string entry);
    const std::vector<std::string>& GetTradeHistory() const { return m_tradeHistory; }

    const std::unordered_set<std::string>& GetAchievements() const { return m_achievements; }
    bool HasAchievement(const std::string& achievement) const;
    void GiveAchievement(const std::string& achievement);
    void SendAchievement(std::string achievementName);

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

    bool CanProcessGamePacket(eGamePacketType packetType);
    bool CanProcessGameMessage();
    bool CanProcessMovePacket(float nextX, float nextY, uint64 nowMS);
    bool CanCollectObjectNow() { return Time::GetSystemTime() - m_lastObjectCollectTime >= 60; }
    void ResetCollectObjectTime() { m_lastObjectCollectTime = Time::GetSystemTime(); }

    void SendPositionToWorldPlayers();

    Timer& GetLastActionTime() { return m_lastActionTime; }
    Timer& GetLastDBSaveTime() { return m_lastDbSaveTime; }

    bool IsLoggingOff() const { return m_loggingOff; }

private:
    uint32 m_state;
    bool m_joiningWorld;
    uint32 m_currentWorldID;

    uint64 m_lastItemActivateTime;
    uint64 m_lastObjectCollectTime;
    Timer m_lastActionTime;

    Timer m_lastCheckGamePacketWindow;
    int32 m_gamePacketsInWindow = 0;

    Timer m_lastCheckGameMessageWindow;
    int32 m_gameMessagesInWindow = 0;

    Timer m_lastCheckMoveWindow;
    int32 m_moveViolationsInWindow = 0;
    uint64 m_lastMovePacketTime = 0;
    Vector2Float m_lastMovePacketPos;
    bool m_hasLastMovePacketPos = false;

    uint32 m_guestID;

    bool m_loggingOff;

    Vector2Float m_respawnPos;
    Vector2Float m_worldPos;

    Timer m_lastDbSaveTime;

    int32 m_gems;

    bool m_isTrading = false;
    bool m_isTradeAccepted = false;
    bool m_isTradeConfirmed = false;
    uint32 m_tradingWithUserID = 0;
    uint64 m_tradeAcceptedAt = 0;
    uint64 m_tradeConfirmedAt = 0;
    uint64 m_lastTradedAt = 0;
    uint64 m_lastChangeTradeDeal = 0;
    uint64 m_lastAcceptedTrade = 0;
    std::vector<TradeOffer> m_tradeOffers = {};
    std::vector<std::string> m_tradeHistory = {};
    std::unordered_set<std::string> m_achievements = {};

    Role* m_pRole;
};