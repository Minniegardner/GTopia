#pragma once

#include "../DatabasePool.h"

static TableQuery sQueryTable[] =
{
    {"SELECT ID FROM Players WHERE Name = '' AND Mac = ? AND PlatformType = ? LIMIT 1;", QUERY_FLAG_RETURN_RESULT},
    {"INSERT INTO Players (GuestName, PlatformType, GuestID, Mac, IP, CreationDate, LastSeenTime) VALUES (?, ?, ?, ?, ?, SYSDATE(), NOW());", QUERY_FLAG_RETURN_INCREMENT},
    {"SELECT GuestID, RoleID, Inventory, Gems, Level, XP, AchievementData, StatisticData, DailyRewardStreak, DailyRewardClaimDay, LastClaimDailyReward FROM Players WHERE ID = ?;", QUERY_FLAG_RETURN_RESULT},
    {"UPDATE Players SET LastSeenTime = NOW(), RoleID = ?, Inventory = ?, Gems = ?, Level = ?, XP = ?, AchievementData = ?, StatisticData = ?, DailyRewardStreak = ?, DailyRewardClaimDay = ?, LastClaimDailyReward = ? WHERE ID = ?;", QUERY_FLAG_NONE},
    {"SELECT ID FROM Players WHERE IP = ?;", QUERY_FLAG_RETURN_RESULT},
    {"SELECT ID FROM Players WHERE Name = '' AND VID = UNHEX(MD5(?)) AND PlatformType = ?;", QUERY_FLAG_RETURN_RESULT},
    {"SELECT ID FROM Players WHERE Name = '' AND GID = UNHEX(MD5(?)) AND PlatformType = ?;", QUERY_FLAG_RETURN_RESULT},
    {"SELECT ID FROM Players WHERE Name = '' AND Hash = ? AND PlatformType = ?;", QUERY_FLAG_RETURN_RESULT},
    {"SELECT (SELECT COUNT(*) FROM Players WHERE IP = ?)  AS ip_count, (SELECT COUNT(*) FROM Players WHERE MAC = ?) AS mac_count,(SELECT COUNT(*) FROM Players WHERE GID = ?) AS gid_count;", QUERY_FLAG_RETURN_RESULT},
    {"SELECT (SELECT COUNT(*) FROM Players WHERE IP = ?)  AS ip_count, (SELECT COUNT(*) FROM Players WHERE MAC = ?) AS mac_count,(SELECT COUNT(*) FROM Players WHERE VID = ?) AS vid_count;", QUERY_FLAG_RETURN_RESULT},
    {"SELECT (SELECT COUNT(*) FROM Players WHERE IP = ?)  AS ip_count, (SELECT COUNT(*) FROM Players WHERE MAC = ?) AS mac_count,(SELECT COUNT(*) FROM Players WHERE SID = ?) AS sid_count;", QUERY_FLAG_RETURN_RESULT},
    {"SELECT (SELECT COUNT(*) FROM Players WHERE IP = ?)  AS ip_count, (SELECT COUNT(*) FROM Players WHERE MAC = ?) AS mac_count;", QUERY_FLAG_RETURN_RESULT},
    {"SELECT ID FROM Players WHERE Name = ? LIMIT 1;", QUERY_FLAG_RETURN_RESULT},
    {"UPDATE Players SET Name = ?, Password = UNHEX(MD5(?)) WHERE ID = ?;"},
    {"SELECT ID FROM Players WHERE Name = ? AND Password = UNHEX(MD5(?));", QUERY_FLAG_RETURN_RESULT}
};

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
    DB_PLAYER_GROWID_RENAME
};

void DatabasePlayerExec(DatabasePool* pPool, ePlayerDBQuery queryID, QueryRequest& req, bool preapred = false);

QueryRequest MakePlayerByMacReq(const string& mac, uint8 platformType, int32 ownerID);
QueryRequest MakePlayerCreateReq(const string& guestName, uint8 platformType, uint16 guestID, const string& mac, const string& ip, int32 ownerID);
QueryRequest MakeGetPlayerDataReq(uint32 userID, int32 ownerID);
QueryRequest MakeSavePlayerReq(uint32 userID, uint32 roleID, const string& inventoryData, int32 gems, uint32 level, uint32 xp, const string& achievementData, const string& statisticData, uint32 dailyRewardStreak, uint32 dailyRewardClaimDay, uint64 lastClaimDailyRewardMs, int32 ownerID);
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

void ExecUpdatePlayerIdentifier(DatabasePool* pPool, uint32 userID, const string& mac, const string& vid, const string& sid, const string& rid, const string& gid, int32 hash, int32 ownerID);