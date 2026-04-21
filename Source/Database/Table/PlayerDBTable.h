#pragma once

#include "../DatabasePool.h"

enum ePlayerDBQuery
{
    DB_PLAYER_GET_BY_MAC,
    DB_PLAYER_CREATE,
    DB_PLAYER_GET_DATA,
    DB_PLAYER_SAVE,
    DB_PLAYER_COUNT_ACC_IP,
    DB_PLAYER_GET_BY_VID,
    DB_PLAYER_GET_BY_GID,
    DB_PLAYER_GET_BY_HASH,
    DB_PLAYER_COUNT_GID_MAC_IP,
    DB_PLAYER_COUNT_VID_MAC_IP,
    DB_PLAYER_COUNT_SID_MAC_IP,
    DB_PLAYER_COUNT_MAC_IP,
    DB_PLAYER_GROWID_EXISTS,
    DB_PLAYER_GROWID_CREATE,
    DB_PLAYER_GET_BY_NAME_AND_PASS,
    DB_PLAYER_GROWID_RENAME,
    DB_PLAYER_FIND_BY_NAME_PREFIX,
    DB_PLAYER_GET_BASIC_BY_ID,
    DB_PLAYER_APPEND_PBAN_STATS_BY_ID,
    DB_PLAYER_GET_SOCIAL_DATA,
    DB_PLAYER_DELETE_FRIENDS,
    DB_PLAYER_INSERT_FRIEND,
    DB_PLAYER_DELETE_FRIEND_REQUESTS,
    DB_PLAYER_INSERT_FRIEND_REQUEST,
    DB_PLAYER_DELETE_IGNORES,
    DB_PLAYER_INSERT_IGNORE
};

void DatabasePlayerExec(DatabasePool* pPool, ePlayerDBQuery queryID, QueryRequest& req, bool preapred = false);

QueryRequest MakePlayerByMacReq(const string& mac, uint8 platformType, int32 ownerID);
QueryRequest MakePlayerCreateReq(const string& guestName, uint8 platformType, uint16 guestID, const string& mac, const string& ip, int32 ownerID);
QueryRequest MakeGetPlayerDataReq(uint32 userID, int32 ownerID);
QueryRequest MakeSavePlayerReq(uint32 userID, uint32 roleID, const string& inventoryData, int32 gems, uint32 level, uint32 xp, const string& achievementData, const string& statisticData, uint32 dailyRewardStreak, uint32 dailyRewardClaimDay, uint64 lastClaimDailyRewardMs, uint64 guildID, bool showLocationToGuild, bool showGuildNotification, bool titleShowPrefix, bool titlePermLegend, bool titlePermGrow4Good, bool titlePermMVP, bool titlePermVIP, bool titleEnabledLegend, bool titleEnabledGrow4Good, bool titleEnabledMVP, bool titleEnabledVIP, uint64 expectedVersionEpochSec, int32 ownerID);
QueryRequest MakeGetForInactive(uint32 userID, int32 ownerID);
QueryRequest MakeCountCreatedAccByIP(const string& ip, int32 ownerID);
QueryRequest MakePlayerByVIDReq(const string& vid, uint8 platformType, int32 ownerID);
QueryRequest MakePlayerByGIDReq(const string& gid, uint8 platformType, int32 ownerID);
QueryRequest MakePlayerByHashReq(int32 hash, uint8 platformType, int32 ownerID);
QueryRequest MakeCountPlayerByGidMacIP(const string& gid, const string& mac, const string& ip, int32 ownerID);
QueryRequest MakeCountPlayerByVidMacIP(const string& vid, const string& mac, const string& ip, int32 ownerID);
QueryRequest MakeCountPlayerBySidMacIP(const string& sid, const string& mac, const string& ip, int32 ownerID);
QueryRequest MakeCountPlayerByMacIP(const string& mac, const string& ip, int32 ownerID);
QueryRequest MakePlayerGrowIDExists(const string& growID, int32 ownerID);
QueryRequest MakePlayerGrowIDCreate(uint32 userID, const string& name, const string& pass, int32 ownerID);
QueryRequest MakeGetPlayerByNameAndPass(const string& name, const string& pass, int32 ownerID);
QueryRequest MakePlayerGrowIDRename(uint32 userID, const string& name, int32 ownerID);
QueryRequest MakeFindPlayersByNamePrefix(const string& prefix, int32 ownerID);
QueryRequest MakeGetPlayerBasicByID(uint32 userID, int32 ownerID);
QueryRequest MakeAppendPlayerPBanStatsByID(uint32 userID, uint32 banUntilEpochSec, uint32 banSetAtEpochSec, uint32 durationSec, int32 ownerID);
QueryRequest MakeGetPlayerSocialDataReq(uint32 userID, int32 ownerID);
QueryRequest MakeDeletePlayerFriendsReq(uint32 userID, int32 ownerID);
QueryRequest MakeInsertPlayerFriendReq(uint32 userID, uint32 friendUserID, int32 ownerID);
QueryRequest MakeDeletePlayerFriendRequestsReq(uint32 userID, int32 ownerID);
QueryRequest MakeInsertPlayerFriendRequestReq(uint32 requesterUserID, uint32 targetUserID, int32 ownerID);
QueryRequest MakeDeletePlayerIgnoresReq(uint32 userID, int32 ownerID);
QueryRequest MakeInsertPlayerIgnoreReq(uint32 userID, uint32 ignoredUserID, int32 ownerID);

void ExecUpdatePlayerIdentifier(DatabasePool* pPool, uint32 userID, const string& mac, const string& vid, const string& sid, const string& rid, const string& gid, int32 hash, int32 ownerID);