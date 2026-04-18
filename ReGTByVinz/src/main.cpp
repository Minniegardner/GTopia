#include "ReGTClient.h"

#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kWindowClassName[] = L"ReGTByVinzMainWindow";
constexpr wchar_t kWindowTitle[] = L"ReGT By Vinz";
constexpr UINT_PTR kUiTimerId = 100;

constexpr int kIdEditHost = 1001;
constexpr int kIdEditPort = 1002;
constexpr int kIdEditGrowId = 1003;
constexpr int kIdEditPassword = 1004;
constexpr int kIdButtonConnect = 1005;
constexpr int kIdButtonLogin = 1006;
constexpr int kIdEditWorld = 1007;
constexpr int kIdButtonJoinWorld = 1008;
constexpr int kIdEditConsole = 1009;
constexpr int kIdLabelState = 1010;

struct AvatarInfo {
    int netID = 0;
    std::wstring name;
    int tileX = 0;
    int tileY = 0;
    bool local = false;
    std::wstring country;
};

struct AppState {
    std::wstring worldName;
    bool worldLoaded = false;
    std::map<int, AvatarInfo> avatars;
};

std::unique_ptr<ReGTClient> g_client;
std::wstring g_consoleBuffer;
AppState g_state;
HWND g_mainWindow = nullptr;
HWND g_hEditHost = nullptr;
HWND g_hEditPort = nullptr;
HWND g_hEditGrowId = nullptr;
HWND g_hEditPassword = nullptr;
HWND g_hEditWorld = nullptr;
HWND g_hEditConsole = nullptr;
HWND g_hLabelState = nullptr;
HWND g_hButtonJoinWorld = nullptr;

std::wstring Utf8ToWide(const std::string& text)
{
    if(text.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    if(size <= 0) {
        return {};
    }

    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

std::string WideToUtf8(const std::wstring& text)
{
    if(text.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if(size <= 0) {
        return {};
    }

    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring GetText(HWND hWnd)
{
    const int length = GetWindowTextLengthW(hWnd);
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    if(length > 0) {
        GetWindowTextW(hWnd, text.data(), length + 1);
        text.resize(static_cast<size_t>(length));
    }
    return text;
}

std::map<std::string, std::string> ParseLines(const std::string& payload)
{
    std::map<std::string, std::string> fields;

    size_t lineStart = 0;
    while(lineStart < payload.size()) {
        const size_t lineEnd = payload.find('\n', lineStart);
        const size_t end = lineEnd == std::string::npos ? payload.size() : lineEnd;
        const std::string line = payload.substr(lineStart, end - lineStart);

        const size_t sep = line.find('|');
        if(sep != std::string::npos) {
            fields[line.substr(0, sep)] = line.substr(sep + 1);
        }

        if(lineEnd == std::string::npos) {
            break;
        }
        lineStart = lineEnd + 1;
    }

    return fields;
}

std::wstring StripColorCodes(const std::wstring& input)
{
    std::wstring out;
    out.reserve(input.size());

    for(size_t i = 0; i < input.size(); ++i) {
        if(input[i] == L'`' && i + 1 < input.size()) {
            ++i;
            continue;
        }
        out.push_back(input[i]);
    }

    return out;
}

void SetState(const std::wstring& state)
{
    if(g_hLabelState) {
        SetWindowTextW(g_hLabelState, state.c_str());
    }
}

void RequestRepaint()
{
    if(g_mainWindow) {
        InvalidateRect(g_mainWindow, nullptr, TRUE);
    }
}

void AppendConsole(const std::wstring& line)
{
    g_consoleBuffer += line;
    g_consoleBuffer += L"\r\n";

    if(g_hEditConsole) {
        SetWindowTextW(g_hEditConsole, g_consoleBuffer.c_str());
        SendMessageW(g_hEditConsole, EM_SETSEL, static_cast<WPARAM>(g_consoleBuffer.size()), static_cast<LPARAM>(g_consoleBuffer.size()));
        SendMessageW(g_hEditConsole, EM_SCROLLCARET, 0, 0);
    }
}

COLORREF ColorFromNetID(int netID)
{
    switch(std::abs(netID) % 6) {
        case 0: return RGB(255, 108, 108);
        case 1: return RGB(110, 210, 120);
        case 2: return RGB(110, 150, 255);
        case 3: return RGB(255, 195, 90);
        case 4: return RGB(214, 110, 255);
        default: return RGB(90, 225, 225);
    }
}

void UpdateAvatarFromSpawn(const std::string& payload)
{
    const auto fields = ParseLines(payload);

    AvatarInfo avatar;
    if(const auto it = fields.find("netID"); it != fields.end()) {
        avatar.netID = std::atoi(it->second.c_str());
    }
    if(const auto it = fields.find("name"); it != fields.end()) {
        avatar.name = Utf8ToWide(it->second);
    }
    if(const auto it = fields.find("country"); it != fields.end()) {
        avatar.country = Utf8ToWide(it->second);
    }
    if(const auto it = fields.find("posXY"); it != fields.end()) {
        const std::string& pos = it->second;
        const size_t sep = pos.find('|');
        if(sep != std::string::npos) {
            avatar.tileX = std::atoi(pos.substr(0, sep).c_str()) / 32;
            avatar.tileY = std::atoi(pos.substr(sep + 1).c_str()) / 32;
        }
    }
    if(const auto it = fields.find("type"); it != fields.end()) {
        avatar.local = it->second == "local";
    }

    if(avatar.netID != 0) {
        g_state.avatars[avatar.netID] = std::move(avatar);
        g_state.worldLoaded = true;
        RequestRepaint();
    }
}

void JoinWorld()
{
    if(!g_client || !g_client->IsConnected()) {
        MessageBoxW(nullptr, L"Connect to server first.", L"ReGT By Vinz", MB_OK | MB_ICONWARNING);
        return;
    }

    const std::wstring world = GetText(g_hEditWorld);
    if(world.empty()) {
        MessageBoxW(nullptr, L"Enter a world name.", L"ReGT By Vinz", MB_OK | MB_ICONWARNING);
        return;
    }

    g_state.worldName = world;
    g_state.worldLoaded = false;
    g_state.avatars.clear();
    g_client->QueueJoinWorld(WideToUtf8(world));
    SetState(L"Joining world...");
}

void OnConnectClicked()
{
    const std::wstring host = GetText(g_hEditHost);
    const std::wstring portText = GetText(g_hEditPort);

    if(host.empty()) {
        MessageBoxW(nullptr, L"Server host is required.", L"ReGT By Vinz", MB_OK | MB_ICONWARNING);
        return;
    }

    uint16_t port = 17196;
    if(!portText.empty()) {
        const int value = _wtoi(portText.c_str());
        if(value > 0 && value < 65536) {
            port = static_cast<uint16_t>(value);
        }
    }

    if(!g_client) {
        g_client = std::make_unique<ReGTClient>();
    }

    const bool started = g_client->Start(
        WideToUtf8(host),
        port,
        [](const std::string& line) {
            AppendConsole(Utf8ToWide(line));
        },
        [](const std::string& eventName, const std::string& payload) {
            if(eventName == "Welcome") {
                AppendConsole(L"[Login] Welcome packet received.");
                return;
            }

            if(eventName == "OnConsoleMessage") {
                AppendConsole(L"[Console] " + StripColorCodes(Utf8ToWide(payload)));
                return;
            }

            if(eventName == "OnDialogRequest") {
                AppendConsole(L"[Dialog] " + StripColorCodes(Utf8ToWide(payload)));
                return;
            }

            if(eventName == "OnTextOverlay") {
                AppendConsole(L"[Overlay] " + StripColorCodes(Utf8ToWide(payload)));
                return;
            }

            if(eventName == "OnSpawn") {
                UpdateAvatarFromSpawn(payload);
                AppendConsole(L"[World] Spawn packet received.");
                return;
            }

            if(eventName == "OnRequestWorldSelectMenu") {
                AppendConsole(L"[World] World select menu ready.");
                return;
            }
        });

    if(!started) {
        MessageBoxW(nullptr, L"Failed to start ENet client.", L"ReGT By Vinz", MB_OK | MB_ICONERROR);
        return;
    }

    SetState(L"Connecting...");
}

void OnLoginClicked()
{
    if(!g_client || !g_client->IsConnected()) {
        MessageBoxW(nullptr, L"Connect to server first.", L"ReGT By Vinz", MB_OK | MB_ICONWARNING);
        return;
    }

    const std::wstring growId = GetText(g_hEditGrowId);
    const std::wstring password = GetText(g_hEditPassword);

    if(growId.empty() || password.empty()) {
        MessageBoxW(nullptr, L"GrowID and password are required.", L"ReGT By Vinz", MB_OK | MB_ICONWARNING);
        return;
    }

    g_client->QueueLogin(WideToUtf8(growId), WideToUtf8(password));
    SetState(L"Login packet sent");
}

void BuildUi(HWND hWnd)
{
    const HFONT hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    CreateWindowW(L"STATIC", L"ReGT By Vinz", WS_VISIBLE | WS_CHILD,
        20, 12, 240, 24, hWnd, nullptr, nullptr, nullptr);

    CreateWindowW(L"STATIC", L"Server Host", WS_VISIBLE | WS_CHILD,
        20, 44, 120, 18, hWnd, nullptr, nullptr, nullptr);
    g_hEditHost = CreateWindowW(L"EDIT", L"127.0.0.1", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        20, 62, 220, 24, hWnd, reinterpret_cast<HMENU>(kIdEditHost), nullptr, nullptr);

    CreateWindowW(L"STATIC", L"Port", WS_VISIBLE | WS_CHILD,
        252, 44, 80, 18, hWnd, nullptr, nullptr, nullptr);
    g_hEditPort = CreateWindowW(L"EDIT", L"17196", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        252, 62, 88, 24, hWnd, reinterpret_cast<HMENU>(kIdEditPort), nullptr, nullptr);

    CreateWindowW(L"STATIC", L"GrowID", WS_VISIBLE | WS_CHILD,
        20, 100, 120, 18, hWnd, nullptr, nullptr, nullptr);
    g_hEditGrowId = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        20, 118, 320, 24, hWnd, reinterpret_cast<HMENU>(kIdEditGrowId), nullptr, nullptr);

    CreateWindowW(L"STATIC", L"Password", WS_VISIBLE | WS_CHILD,
        20, 156, 120, 18, hWnd, nullptr, nullptr, nullptr);
    g_hEditPassword = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        20, 174, 320, 24, hWnd, reinterpret_cast<HMENU>(kIdEditPassword), nullptr, nullptr);
    SendMessageW(g_hEditPassword, EM_SETPASSWORDCHAR, static_cast<WPARAM>(L'*'), 0);

    CreateWindowW(L"BUTTON", L"Connect", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        20, 214, 150, 30, hWnd, reinterpret_cast<HMENU>(kIdButtonConnect), nullptr, nullptr);

    CreateWindowW(L"BUTTON", L"Login", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        190, 214, 150, 30, hWnd, reinterpret_cast<HMENU>(kIdButtonLogin), nullptr, nullptr);

    CreateWindowW(L"STATIC", L"World", WS_VISIBLE | WS_CHILD,
        20, 254, 120, 18, hWnd, nullptr, nullptr, nullptr);
    g_hEditWorld = CreateWindowW(L"EDIT", L"START", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        20, 272, 160, 24, hWnd, reinterpret_cast<HMENU>(kIdEditWorld), nullptr, nullptr);

    g_hButtonJoinWorld = CreateWindowW(L"BUTTON", L"Join World", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        190, 270, 150, 28, hWnd, reinterpret_cast<HMENU>(kIdButtonJoinWorld), nullptr, nullptr);

    g_hLabelState = CreateWindowW(L"STATIC", L"Idle", WS_VISIBLE | WS_CHILD,
        20, 312, 320, 20, hWnd, reinterpret_cast<HMENU>(kIdLabelState), nullptr, nullptr);

    CreateWindowW(L"STATIC", L"Server Console", WS_VISIBLE | WS_CHILD,
        370, 12, 300, 20, hWnd, nullptr, nullptr, nullptr);
    g_hEditConsole = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL,
        370, 32, 520, 380, hWnd, reinterpret_cast<HMENU>(kIdEditConsole), nullptr, nullptr);

    const std::vector<HWND> controls = {
        g_hEditHost, g_hEditPort, g_hEditGrowId, g_hEditPassword,
        g_hEditWorld, g_hButtonJoinWorld, g_hLabelState, g_hEditConsole,
        GetDlgItem(hWnd, kIdButtonConnect), GetDlgItem(hWnd, kIdButtonLogin)
    };

    for(HWND ctrl : controls) {
        if(ctrl) {
            SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        }
    }
}

void DrawAvatar(HDC hdc, const AvatarInfo& avatar, int originX, int originY)
{
    const int x = originX + (std::abs(avatar.tileX) % 9) * 32;
    const int y = originY + (std::abs(avatar.tileY) % 4) * 32;
    const COLORREF color = ColorFromNetID(avatar.netID);

    HBRUSH fill = CreateSolidBrush(color);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, fill));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(20, 20, 20));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));

    Ellipse(hdc, x + 9, y + 1, x + 23, y + 15);
    RoundRect(hdc, x + 8, y + 14, x + 24, y + 31, 5, 5);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(fill);
    DeleteObject(pen);

    std::wstring label = avatar.name.empty() ? L"Player" : avatar.name;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(245, 245, 245));
    TextOutW(hdc, x - 6, y + 35, label.c_str(), static_cast<int>(label.size()));
}

void PaintScene(HWND hWnd, HDC hdc)
{
    RECT rc{};
    GetClientRect(hWnd, &rc);

    RECT worldRect{20, 352, 340, 500};
    HBRUSH worldBg = CreateSolidBrush(RGB(22, 26, 36));
    FillRect(hdc, &worldRect, worldBg);
    DeleteObject(worldBg);

    HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(42, 52, 68));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, gridPen));
    for(int x = worldRect.left; x <= worldRect.right; x += 32) {
        MoveToEx(hdc, x, worldRect.top, nullptr);
        LineTo(hdc, x, worldRect.bottom);
    }
    for(int y = worldRect.top; y <= worldRect.bottom; y += 32) {
        MoveToEx(hdc, worldRect.left, y, nullptr);
        LineTo(hdc, worldRect.right, y);
    }
    SelectObject(hdc, oldPen);
    DeleteObject(gridPen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(235, 235, 235));
    const std::wstring worldTitle = L"World: " + (g_state.worldName.empty() ? L"<none>" : g_state.worldName);
    TextOutW(hdc, 20, 330, worldTitle.c_str(), static_cast<int>(worldTitle.size()));

    const std::wstring stateLine = g_state.worldLoaded ? L"State: world loaded" : L"State: waiting map/spawn data";
    TextOutW(hdc, 20, 346, stateLine.c_str(), static_cast<int>(stateLine.size()));

    if(g_state.avatars.empty()) {
        const std::wstring hint = L"Join a world to see avatar placeholders here.";
        TextOutW(hdc, 20, 372, hint.c_str(), static_cast<int>(hint.size()));
    }

    for(const auto& [netID, avatar] : g_state.avatars) {
        (void)netID;
        DrawAvatar(hdc, avatar, worldRect.left + 12, worldRect.top + 12);
    }
}

LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message) {
        case WM_CREATE:
            g_mainWindow = hWnd;
            BuildUi(hWnd);
            SetTimer(hWnd, kUiTimerId, 250, nullptr);
            return 0;

        case WM_TIMER:
            if(wParam == kUiTimerId && g_client && g_client->IsConnected()) {
                SetState(g_client->IsHelloReceived() ? L"Connected (hello ok)" : L"Connected (waiting hello)");
                RequestRepaint();
            }
            return 0;

        case WM_COMMAND: {
            const int controlId = LOWORD(wParam);
            const int eventCode = HIWORD(wParam);
            if(eventCode == BN_CLICKED) {
                if(controlId == kIdButtonConnect) {
                    OnConnectClicked();
                    return 0;
                }
                if(controlId == kIdButtonLogin) {
                    OnLoginClicked();
                    return 0;
                }
                if(controlId == kIdButtonJoinWorld) {
                    JoinWorld();
                    return 0;
                }
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT client{};
            GetClientRect(hWnd, &client);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, client.right, client.bottom);
            HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, memBmp));

            HBRUSH bg = CreateSolidBrush(RGB(14, 18, 24));
            FillRect(memDC, &client, bg);
            DeleteObject(bg);

            PaintScene(hWnd, memDC);

            BitBlt(hdc, 0, 0, client.right, client.bottom, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hWnd, kUiTimerId);
            if(g_client) {
                g_client->Stop();
            }
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    WNDCLASSW wc{};
    wc.lpfnWndProc = MainWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kWindowClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if(!RegisterClassW(&wc)) {
        MessageBoxW(nullptr, L"Window registration failed.", kWindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND hWnd = CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        940,
        520,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if(!hWnd) {
        MessageBoxW(nullptr, L"Window creation failed.", kWindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg{};
    while(GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
