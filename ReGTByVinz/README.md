# ReGT By Vinz

ReGT By Vinz is a new Windows Growtopia-style client scaffold with old-client inspired UI and working login flow for this repository protocol.

Current capabilities:
- ENet connection to GameServer.
- Handles server hello handshake.
- Sends login packet (GrowID + password + required identity fields).
- Parses and displays:
  - text packets (`action|log`, `action|logon_fail`),
  - call-function packets (`OnConsoleMessage`, `OnDialogRequest`, `OnTextOverlay`, `OnSendToServer`).
- Auto-sends `refresh_item_data`, `refresh_player_tribute_data`, and `enter_game` after welcome callback.
- Supports joining worlds from the UI and renders a simple world scene with avatar placeholders.

## Build

```powershell
cd ReGTByVinz
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Run:
- `ReGTByVinz/build/Release/ReGTByVinz.exe`

## Auto Setup

From the repository root, run:

```bat
fiel.bat
```

That script updates submodules, configures the client project, and builds it.

## Notes

- This is a full login and message pipeline foundation.
- World rendering, inventory scene, and full gameplay renderer are separate milestones.
- The current world scene is intentionally lightweight and uses simple avatar placeholders until packet parsing and renderer parity are expanded.
