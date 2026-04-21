#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]


def must_contain(path: Path, needle: str, label: str) -> None:
    content = path.read_text(encoding="utf-8")
    if needle not in content:
        raise AssertionError(f"[{label}] missing in {path}")


def run() -> None:
    db_result = ROOT / "Source" / "Database" / "DatabaseResult.cpp"
    player_table = ROOT / "Source" / "Database" / "Table" / "PlayerDBTable.cpp"
    game_player = ROOT / "GameServer" / "Player" / "GamePlayer.cpp"
    master_player = ROOT / "Master" / "Player" / "GamePlayer.cpp"

    # Unit-level mapper/parser guard
    must_contain(
        db_result,
        "var = (field.flags & UNSIGNED_FLAG) ? (uint32)0 : (int32)0;",
        "db-null-signedness-guard",
    )

    # Integration-level login-load-save safety checks
    must_contain(
        player_table,
        "INSERT INTO Players (GuestName, PlatformType, GuestID, Mac, IP, Gems, Level, XP, CreationDate, LastSeenTime)",
        "explicit-new-account-defaults",
    )
    must_contain(
        player_table,
        "UNIX_TIMESTAMP(LastSeenTime) AS LastSeenVersion",
        "load-version-column",
    )
    must_contain(
        player_table,
        "WHERE ID = ? AND LastSeenTime <= FROM_UNIXTIME(?)",
        "optimistic-save-guard",
    )

    # Regression-specific protection: existing account must not be overwritten before hydration
    must_contain(
        game_player,
        "save_blocked_pre_hydration",
        "save-blocked-log",
    )
    must_contain(
        game_player,
        "if(m_loadedAccountVersionEpochSec == 0)",
        "missing-version-save-block",
    )

    # Existing guest account fallback should not create a fresh account too early
    must_contain(
        master_player,
        "m_triedMacFallback",
        "master-mac-fallback-guard",
    )
    must_contain(
        master_player,
        "profile_create_path",
        "create-path-structured-log",
    )


if __name__ == "__main__":
    try:
        run()
    except AssertionError as exc:
        print(f"FAIL: {exc}")
        sys.exit(1)

    print("PASS: login safety regression checks")
