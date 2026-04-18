# GrowClientWin (MVP)

Minimal Windows client scaffold for a new Growtopia-style client.

Current MVP:
- Native Win32 login window
- Input for server host, GrowID, and password
- Basic validation and login status feedback

## Build (Windows, CMake)

```powershell
cd GrowClientWin
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Run:
- `GrowClientWin/build/Release/GrowClientWin.exe`

## Notes

This MVP does not yet connect to server sockets. It is intentionally focused on first-login UI flow and project foundation.
