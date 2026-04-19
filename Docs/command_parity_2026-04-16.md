# Command Parity Audit (Reference vs Current Source)

This document maps commands found in:
- Reference: `ReferensiSrc/GrowtopiaV1/Src/CommandSystem/Commands/*.hh`
- Current source: `GameServer/Command/*.cpp`

## Overlap (present in both)

- Ban (`/ban`)
- Effect (`/effect`)
- Find (`/find`)
- Ghost (`/ghost`)
- Help (`/help`)
- InvSee (`/invsee`)
- ItemInfo (`/iteminfo`)
- Kick (`/kick`)
- KickAll (`/kickall`)
- Magic (`/magic`)
- Message/Msg (`/msg`)
- News (`/news`)
- Online (`/online`)
- PInfo (`/pinfo`)
- Ping (`/ping`)
- Pull (`/pull`)
- RandomPull (`/randompull`)
- RenderWorld (`/renderworld`)
- ReplaceBlocks (`/replaceblocks`)
- Reply (`/reply`)
- SetGems (`/setgems`)
- Stats (`/stats`)
- Summon (`/summon`)
- SuperBroadcast (`/sb` or equivalent)
- Suspend (`/suspend`)
- Time (`/time`)
- Uba/Unban (`/uba`)
- Warn (`/warn`)
- Warp (`/warp`)
- WarpTo (`/warpto`)
- Weather (`/weather`)
- Who (`/who`)

## Present in current source but not in reference list (or naming differs)

- GiveItem (`/giveitem`) [custom permissioned utility]
- GrowIDCmd (`/growid` flow utility)
- Mod (`/mod`) [moderation dialog entrypoint]
- TogglePlayMod (`/toggleplaymod`) [playmod toggle utility]
- Trade (`/trade`) [custom utility]

## Present in reference but missing in current source

- AnitaMaxWynn
- Beta
- Break
- Cache
- Cheat
- ClearSurgStuff
- DB
- Destructo
- Dialog
- Drop
- EFlag
- Extend
- GemEvent
- IPS
- LoadTileSequence
- LoadWorld
- MakeFullPlat
- MoveDoor
- Nick
- Packet
- Punch
- ReloadCurrentWorld
- ResetWorld
- Spawn
- Store
- Clear (partial: `drop` mode)
- Warp (if behavior differs from current implementation)

## Notes for message/dialog parity

- Command name overlap does not guarantee behavior parity.
- Priority for parity pass should be:
  1. Moderation commands (`kick`, `ban`, `warn`, `suspend`, `pull`, `summon`, `uba`).
  2. Utility info commands (`pinfo`, `who`, `online`, `news`, `help`).
  3. World interaction commands (`warp`, `warpto`, `weather`, `renderworld`, `replaceblocks`).

## Changes made in this pass

- Enter-game console messaging aligned to reference phrasing for:
  - Welcome line format.
  - Friends-online line format.
  - World-select prompt (`others online`).
- Join-world lock message format aligned with reference color/style.
- `setgems` permission moved off generic moderator permission and tied to `command_giveitem` permission.
- Moderator role expanded to broad mod command access while still excluding `giveitem`-gated functionality.
