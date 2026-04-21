-- Pre-deploy snapshot for login data safety patch
-- Run this before deploying write-path changes.

CREATE TABLE IF NOT EXISTS Players_backup_login_fix_20260421 AS
SELECT * FROM Players;

-- Quick sanity checks after deployment.
-- 1) Existing accounts unexpectedly reverted to defaults.
SELECT ID, Name, GuestName, Gems, Level, XP, LastSeenTime
FROM Players
WHERE LastSeenTime >= DATE_SUB(NOW(), INTERVAL 1 DAY)
  AND Gems = 0
  AND Level = 1
  AND XP = 0
  AND Name <> '';

-- 2) Potential duplicate guest identity by hash.
SELECT Hash, COUNT(*) AS dup_count
FROM Players
WHERE Name = '' AND Hash <> 0
GROUP BY Hash
HAVING COUNT(*) > 1;
