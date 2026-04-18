CREATE TABLE `Players` (
  `ID` int NOT NULL AUTO_INCREMENT,
  `GuestName` varchar(32) NOT NULL,
  `CreationDate` date DEFAULT NULL,
  `Mac` char(24) DEFAULT NULL,
  `GuestID` int DEFAULT '0',
  `IP` char(15) NOT NULL DEFAULT '0.0.0.0',
  `Flags` int NOT NULL DEFAULT '0',
  `Name` varchar(32) DEFAULT '',
  `Password` binary(16) DEFAULT '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0',
  `LastSeenTime` datetime DEFAULT NULL,
  `RID` binary(16) NOT NULL DEFAULT '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0',
  `PlatformType` smallint NOT NULL DEFAULT '-1',
  `GID` binary(16) NOT NULL DEFAULT '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0',
  `Inventory` varbinary(512) DEFAULT NULL,
  `Gems` int NOT NULL DEFAULT '0',
  `Level` int NOT NULL DEFAULT '1',
  `XP` int NOT NULL DEFAULT '0',
  `AchievementData` longtext DEFAULT NULL,
  `StatisticData` longtext DEFAULT NULL,
  `DailyRewardStreak` int NOT NULL DEFAULT '0',
  `DailyRewardClaimDay` int NOT NULL DEFAULT '0',
  `LastClaimDailyReward` bigint NOT NULL DEFAULT '0',
  `RoleID` int DEFAULT NULL,
  `GuildID` bigint unsigned NOT NULL DEFAULT '0',
  `ShowLocationToGuild` tinyint(1) NOT NULL DEFAULT '1',
  `ShowGuildNotification` tinyint(1) NOT NULL DEFAULT '1',
  `TitleShowPrefix` tinyint(1) NOT NULL DEFAULT '1',
  `TitlePermLegend` tinyint(1) NOT NULL DEFAULT '0',
  `TitlePermGrow4Good` tinyint(1) NOT NULL DEFAULT '0',
  `TitlePermMVP` tinyint(1) NOT NULL DEFAULT '0',
  `TitlePermVIP` tinyint(1) NOT NULL DEFAULT '0',
  `TitleEnabledLegend` tinyint(1) NOT NULL DEFAULT '0',
  `TitleEnabledGrow4Good` tinyint(1) NOT NULL DEFAULT '0',
  `TitleEnabledMVP` tinyint(1) NOT NULL DEFAULT '0',
  `TitleEnabledVIP` tinyint(1) NOT NULL DEFAULT '0',
  `VID` binary(16) NOT NULL DEFAULT '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0',
  `Hash` int NOT NULL DEFAULT '0',
  `SID` binary(16) NOT NULL DEFAULT '\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0',
  PRIMARY KEY (`ID`),
  KEY `idx_gid` (`GID`),
  KEY `idx_rid` (`RID`),
  KEY `idx_ip` (`IP`),
  KEY `idx_vid` (`VID`),
  KEY `idx_hash` (`Hash`),
  KEY `idx_sid` (`SID`),
  KEY `idx_name` (`Name`),
  KEY `idx_guildid` (`GuildID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `Guilds` (
  `ID` bigint unsigned NOT NULL,
  `Name` varchar(18) NOT NULL,
  `Statement` varchar(50) NOT NULL DEFAULT '',
  `Notebook` longtext,
  `WorldID` int unsigned NOT NULL DEFAULT '0',
  `OwnerID` int unsigned NOT NULL DEFAULT '0',
  `Level` int unsigned NOT NULL DEFAULT '1',
  `XP` int unsigned NOT NULL DEFAULT '0',
  `LogoFG` int NOT NULL DEFAULT '0',
  `LogoBG` int NOT NULL DEFAULT '0',
  `ShowMascot` tinyint(1) NOT NULL DEFAULT '1',
  `CreatedAt` bigint unsigned NOT NULL DEFAULT '0',
  `MemberCount` int unsigned NOT NULL DEFAULT '0',
  `MemberData` longblob,
  PRIMARY KEY (`ID`),
  UNIQUE KEY `uq_guild_name` (`Name`),
  KEY `idx_owner` (`OwnerID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `Worlds` (
  `ID` int NOT NULL AUTO_INCREMENT,
  `Name` varchar(64) NOT NULL,
  `HomeServerID` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `LastSeenTime` datetime DEFAULT NULL,
  `Version` int NOT NULL DEFAULT '0',
  `CreationDate` date DEFAULT NULL,
  PRIMARY KEY (`ID`),
  KEY `idx_home_server` (`HomeServerID`),
  KEY `idx_name` (`Name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `PlayerFriends` (
  `UserID` int NOT NULL,
  `FriendUserID` int NOT NULL,
  `CreatedAt` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`UserID`, `FriendUserID`),
  KEY `idx_friend_user` (`FriendUserID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `PlayerFriendRequests` (
  `RequesterUserID` int NOT NULL,
  `TargetUserID` int NOT NULL,
  `CreatedAt` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`RequesterUserID`, `TargetUserID`),
  KEY `idx_friend_request_target` (`TargetUserID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `PlayerIgnoredUsers` (
  `UserID` int NOT NULL,
  `IgnoredUserID` int NOT NULL,
  `CreatedAt` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`UserID`, `IgnoredUserID`),
  KEY `idx_ignored_user` (`IgnoredUserID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
