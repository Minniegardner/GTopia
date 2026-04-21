#pragma once

#include "Player/Player.h"
#include "Player/Role.h"
#include "Utils/Timer.h"
#include "Player/PlayMod.h"
#include "Packet/GamePacket.h"
#include "../Guild/Guild.h"

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
    PLAYER_STATE_RENDERING_WORLD = 1 << 6,
    PLAYER_STATE_CREATING_GROWID = 1 << 7,
    PLAYER_STATE_LOADING_SOCIAL = 1 << 8,
    PLAYER_STATE_LOADING_GUILD = 1 << 9
};

enum ePlayerSubState
{
    PLAYER_SUB_GROWID_CHECK_IDENTIFIERS,
    PLAYER_SUB_GROWID_CHECK_NAME,
    PLAYER_SUB_GROWID_SUCCESS,
    PLAYER_SUB_PBAN_BY_PREFIX,
    PLAYER_SUB_PBAN_BY_USERID,
    PLAYER_SUB_SOCIAL_LOAD,
    PLAYER_SUB_GUILD_LOAD,
    PLAYER_SUB_GUILD_NAME_CHECK,
    PLAYER_SUB_GUILD_CREATE
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
    void HandlePBanRequestResult(QueryTaskResult&& result);
    void FinalizePBanResolvedTarget(uint32 targetUserID, const string& resolvedTargetName);

    void SaveToDatabase();
    void LogOff();

    void Update();

    void SetJoinWorld(bool joining) { m_joiningWorld = joining; }
    bool IsJoiningWorld() { return m_joiningWorld; }
    
    void SetCurrentWorld(uint32 worldID) {
        if(m_currentWorldID != 0 && m_currentWorldID != worldID) {
            m_pendingGuildInvites.clear();
        }

        m_currentWorldID = worldID;
    }
    uint32 GetCurrentWorld() const { return m_currentWorldID; }

    void SetSwitchingSubserver(bool switching) { m_switchingSubserver = switching; }
    bool IsSwitchingSubserver() const { return m_switchingSubserver; }

    string GetDisplayName();
    string GetRawName();
    void SetNickname(const string& nickname) { m_nickname = nickname; }
    const string& GetNickname() const { return m_nickname; }
    bool HasNickname() const { return !m_nickname.empty(); }
    string GetSpawnData(bool local);
    bool SetRoleByID(int32 roleID);

    bool IsPrefixEnabled() const;
    void SetPrefixEnabled(bool enabled);
    bool IsLegend() const;
    void SetIsLegend(bool enabled);
    bool IsGrow4Good() const;
    void SetIsGrow4Good(bool enabled);
    bool IsMVP() const;
    void SetIsMVP(bool enabled);
    bool IsVIP() const;
    void SetIsVIP(bool enabled);
    bool IsLegendaryTitleEnabled();
    void SetLegendaryTitleEnabled(bool enabled);
    bool IsGrow4GoodTitleEnabled();
    void SetGrow4GoodTitleEnabled(bool enabled);
    bool IsMaxLevelTitleEnabled();
    void SetMaxLevelTitleEnabled(bool enabled);
    bool IsSuperSupporterTitleEnabled();
    void SetSuperSupporterTitleEnabled(bool enabled);

    void SetWorldPos(float x, float y) { m_worldPos.x = x; m_worldPos.y = y; }
    Vector2Float GetWorldPos() const { return m_worldPos; }

    uint32 GetHealth() const { return m_health; }
    void SetHealth(uint32 value) { m_health = std::min<uint32>(200, value); }
    uint64 GetLastDamageMS() const { return m_lastDamageMS; }
    void SetLastDamageMS(uint64 value) { m_lastDamageMS = value; }

    void SetRespawnPos(float x, float y) { m_respawnPos.x = x; m_respawnPos.y = y; }
    Vector2Float GetRespawnPos() const { return m_respawnPos; }

    Vector2Int GetMagplantPos() const { return m_magplantPos; }
    void SetMagplantPos(Vector2Int pos);
    void SetPendingDoorWarpID(const string& doorID) { m_pendingDoorWarpID = doorID; }
    string ConsumePendingDoorWarpID() { string doorID = m_pendingDoorWarpID; m_pendingDoorWarpID.clear(); return doorID; }
    void SetBirthCertificatePendingName(const string& name) { m_birthCertificatePendingName = name; }
    const string& GetBirthCertificatePendingName() const { return m_birthCertificatePendingName; }
    void ClearBirthCertificatePendingName() { m_birthCertificatePendingName.clear(); }

    uint32 GetDailyRewardStreak() const { return m_dailyRewardStreak; }
    void SetDailyRewardStreak(uint32 streak) { m_dailyRewardStreak = streak; }

    uint32 GetDailyRewardClaimDay() const { return m_dailyRewardClaimDay; }
    void SetDailyRewardClaimDay(uint32 day) { m_dailyRewardClaimDay = day; }

    uint64 GetLastClaimDailyReward() const { return m_lastClaimDailyRewardMs; }
    void SetLastClaimDailyReward(uint64 timeMs) { m_lastClaimDailyRewardMs = timeMs; }

    bool CanClaimDailyReward(uint32 currentEpochDay) const;
    void ResetDailyRewardProgressIfNewDay(uint32 currentEpochDay);

    int32 GetGems() const { return m_gems; }
    void SetGems(int32 gems) { m_gems = std::max(0, gems); }
    bool TrySpendGems(int32 amount);
    void AddGems(int32 amount);
    void SendOnSetBux();

    uint32 GetLevel() const { return m_level; }
    void SetLevel(uint32 level) { m_level = std::max<uint32>(1, level); }
    uint32 GetXP() const { return m_xp; }
    void SetXP(uint32 xp) { m_xp = xp; }
    void AddXP(uint32 amount);

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
    uint16 GetPendingTradeModifyItemID() const { return m_pendingTradeModifyItemID; }
    void SetPendingTradeModifyItemID(uint16 itemID) { m_pendingTradeModifyItemID = itemID; }
    void ClearPendingTradeModifyItemID() { m_pendingTradeModifyItemID = 0; }
    uint16 GetPendingFindItemID() const { return m_pendingFindItemID; }
    void SetPendingFindItemID(uint16 itemID) { m_pendingFindItemID = itemID; }
    void ClearPendingFindItemID() { m_pendingFindItemID = 0; }
    uint64 GetLastAcceptedTrade() const { return m_lastAcceptedTrade; }
    void SetLastAcceptedTrade(uint64 timeMS) { m_lastAcceptedTrade = timeMS; }
    uint32 GetPendingTradeRequesterUserID() const { return m_pendingTradeRequesterUserID; }
    uint64 GetPendingTradeRequestedAtMS() const { return m_pendingTradeRequestedAtMS; }
    void SetPendingTradeRequest(uint32 requesterUserID, uint64 requestedAtMS);
    void ClearPendingTradeRequest();
    bool HasPendingTradeRequestFrom(uint32 requesterUserID, uint64 nowMS) const;
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

    void SendWrenchSelf(std::string page = "PlayerInfo");
    void SendWrenchOthers(GamePlayer* otherPlayer);
    void SendFriendMenu(const string& action = "GotoFriendsMenu");
    void SendSocialPortal();

    bool IsFriendWith(uint32 userID) const;
    uint32 CountOnlineFriends() const;
    bool IsShowFriendNotification() const;
    void SetShowFriendNotification(bool enabled);
    bool ShouldProcessFriendAlert(uint32 sourceUserID, bool loggedIn, uint64 nowMS);
    void NotifyFriendsStatusChange(bool loggedIn);
    bool IsFriendRequestSentTo(uint32 userID) const;
    bool IsFriendRequestReceivedFrom(uint32 userID) const;
    void SendFriendRequestTo(GamePlayer* otherPlayer);
    bool AcceptFriendRequestFrom(GamePlayer* otherPlayer);
    void DenyFriendRequestFrom(uint32 userID);
    bool AddFriendUserID(uint32 userID);
    bool RemoveFriendUserID(uint32 userID);
    bool IsIgnoring(uint32 userID) const;
    bool AddIgnoredUserID(uint32 userID);
    bool RemoveIgnoredUserID(uint32 userID);

    uint64 GetGuildID() const { return m_guildID; }
    void SetGuildID(uint64 guildID) { m_guildID = guildID; }
    bool IsShowLocationToGuild() const { return m_showLocationToGuild; }
    void SetShowLocationToGuild(bool val) { m_showLocationToGuild = val; }
    bool IsShowGuildNotification() const { return m_showGuildNotification; }
    void SetShowGuildNotification(bool val) { m_showGuildNotification = val; }
    bool IsGuildMascotEnabled() const { return m_isGuildMascotEnabled && m_guildID != 0; }
    void SetGuildMascotEnabled(bool val) { m_isGuildMascotEnabled = val; }
    void SetPendingGuildCreationData(const string& guildName, const string& guildStatement) { m_pendingGuildName = guildName; m_pendingGuildStatement = guildStatement; }
    bool CreateGuildFromPendingData();
    void SendGuildMenu(const string& button = "");
    void SendGuildDataChanged(Guild* pGuild = nullptr);
    void AddPendingGuildInvite(uint64 guildID, Guild* pGuild);
    void RemovePendingGuildInvite(uint64 guildID);
    Guild* GetPendingGuildInvite(uint64 guildID) const;
    void SendPendingGuildInviteDialog();
    void HandleGuildInviteResponse(uint64 guildID, bool accepted);

    bool IsMuted() const;
    uint64 GetMutedUntilMS() const { return m_mutedUntilMS; }
    const string& GetMuteReason() const { return m_muteReason; }
    void SetMutedUntilMS(uint64 untilMS, const string& reason = "");
    void ClearMute();

    const std::unordered_set<std::string>& GetAchievements() const { return m_achievements; }
    bool HasAchievement(const std::string& achievement) const;
    void GiveAchievement(const std::string& achievement);
    void SendAchievement(std::string achievementName);
    std::string SerializeAchievements() const;
    std::string SerializeStatistics() const;
    void LoadAchievements(const std::string& data);
    void LoadStatistics(const std::string& data);
    void IncreaseStat(const std::string& statName, uint64 amount = 1);
    uint64 GetStatCount(const std::string& statName) const;
    uint64 GetCustomStatValue(const std::string& statName) const;
    void SetCustomStatValue(const std::string& statName, uint64 value);

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
    bool CanCollectObjectNow() { return Time::GetSystemTime() - m_lastObjectCollectTime >= 25; }
    void ResetCollectObjectTime() { m_lastObjectCollectTime = Time::GetSystemTime(); }
    uint64 GetLastCollectFailCheckTime() const { return m_lastCollectFailCheckTime; }
    uint8 GetCollectFailsInWindow() const { return m_collectFailsInWindow; }
    void ResetCollectFailWindow(uint64 nowMS) { m_lastCollectFailCheckTime = nowMS; m_collectFailsInWindow = 0; }
    void IncrementCollectFailWindow() { ++m_collectFailsInWindow; }
    bool CanTriggerSteamByStep(const Vector2Int& tilePos, uint64 nowMS);
    bool HasGrowID() { return !m_loginDetail.tankIDPass.empty(); }
    void ExecGrowIDIdentifierCheck(bool fromDialog, const VariantVector& extraData = VariantVector{});
    void BeginPBanRequest(const string& target, int32 durationSec, const string& reason);
    void ApplyAccountBan(int32 durationSec, const string& reason, const string& sourceRawName, bool sendWorldMessage);

    void SendPositionToWorldPlayers();

    Timer& GetLastActionTime() { return m_lastActionTime; }
    Timer& GetLastDBSaveTime() { return m_lastDbSaveTime; }

    bool IsLoggingOff() const { return m_loggingOff; }
    uint32 GetLastWhisperUserID() const { return m_lastWhisperUserID; }
    void SetLastWhisperUserID(uint32 userID) { m_lastWhisperUserID = userID; }
    uint64 GetLastWhisperAtMS() const { return m_lastWhisperAtMS; }
    void SetLastWhisperAtMS(uint64 timeMS) { m_lastWhisperAtMS = timeMS; }

    void ResetGauntletSwapState();
    void SetGauntletSwapIndex(int32 slot, int32 tileIndex);
    int32 GetGauntletSwapIndex(int32 slot) const;
    void SetGauntletAvailableSwap(const std::vector<int32>& tiles);
    const std::vector<int32>& GetGauntletAvailableSwap() const { return m_gauntletAvailableSwap; }

private:
    void RequestGuildDataLoad();
    void HandleGuildDataLoad(QueryTaskResult&& result);
    void SyncSocialDataToStats();
    void LoadSocialDataFromStats();
    void RequestSocialDataLoad();
    void HandleSocialDataLoad(QueryTaskResult&& result);
    void SaveSocialDataToDatabase();

private:
    struct PendingPBanRequest {
        bool active = false;
        int32 durationSec = 0;
        string reason;
        string target;
    };

private:
    uint32 m_state;
    bool m_joiningWorld;
    uint32 m_currentWorldID;

    uint64 m_lastItemActivateTime;
    uint64 m_lastObjectCollectTime;
    uint64 m_lastCollectFailCheckTime = 0;
    uint8 m_collectFailsInWindow = 0;
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

    uint64 m_lastSteamStepTriggerTime = 0;
    Vector2Int m_lastSteamStepTilePos = { -1, -1 };

    uint32 m_guestID;

    bool m_loggingOff;
    bool m_accountDataLoaded = false;
    bool m_switchingSubserver = false;
    uint64 m_loadedAccountVersionEpochSec = 0;

    Vector2Float m_respawnPos;
    Vector2Float m_worldPos;
    uint32 m_health = 200;
    uint64 m_lastDamageMS = 0;
    Vector2Int m_magplantPos = { -1, -1 };
    string m_pendingDoorWarpID = "";
    string m_birthCertificatePendingName = "";
    uint16 m_pendingTradeModifyItemID = 0;
    uint16 m_pendingFindItemID = 0;
    string m_nickname = "";

    Timer m_lastDbSaveTime;

    int32 m_gems;
    uint32 m_level = 1;
    uint32 m_xp = 0;

    bool m_isTrading = false;
    bool m_isTradeAccepted = false;
    bool m_isTradeConfirmed = false;
    uint32 m_tradingWithUserID = 0;
    uint64 m_tradeAcceptedAt = 0;
    uint64 m_tradeConfirmedAt = 0;
    uint64 m_lastTradedAt = 0;
    uint64 m_lastChangeTradeDeal = 0;
    uint64 m_lastAcceptedTrade = 0;
    uint32 m_pendingTradeRequesterUserID = 0;
    uint64 m_pendingTradeRequestedAtMS = 0;
    std::vector<TradeOffer> m_tradeOffers = {};
    std::vector<std::string> m_tradeHistory = {};
    std::unordered_set<std::string> m_achievements = {};
    std::unordered_map<std::string, uint64> m_stats = {};
    std::unordered_set<uint32> m_friendUserIDs = {};
    std::unordered_set<uint32> m_sentFriendRequestUserIDs = {};
    std::unordered_set<uint32> m_receivedFriendRequestUserIDs = {};
    std::unordered_set<uint32> m_ignoredUserIDs = {};
    std::unordered_map<uint64, uint64> m_friendAlertDebounce = {};
    uint64 m_guildID = 0;
    bool m_showLocationToGuild = true;
    bool m_showGuildNotification = true;
    bool m_titleShowPrefix = true;
    bool m_titlePermLegend = false;
    bool m_titlePermGrow4Good = false;
    bool m_titlePermMVP = false;
    bool m_titlePermVIP = false;
    bool m_titleEnabledLegend = false;
    bool m_titleEnabledGrow4Good = false;
    bool m_titleEnabledMVP = false;
    bool m_titleEnabledVIP = false;
    bool m_isGuildMascotEnabled = false;
    string m_pendingGuildName = "";
    string m_pendingGuildStatement = "";
    uint32 m_lastWhisperUserID = 0;
    std::unordered_map<uint64, Guild*> m_pendingGuildInvites = {};
    uint64 m_lastWhisperAtMS = 0;
    uint64 m_mutedUntilMS = 0;
    string m_muteReason = "";

    int32 m_gauntletSwapIndex[2] = { -1, -1 };
    std::vector<int32> m_gauntletAvailableSwap = {};

    uint32 m_dailyRewardStreak = 0;
    uint32 m_dailyRewardClaimDay = 0;
    uint64 m_lastClaimDailyRewardMs = 0;

    PendingPBanRequest m_pendingPBan;

    Role* m_pRole;
};