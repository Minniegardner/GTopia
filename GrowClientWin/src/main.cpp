#include <windows.h>

#include <string>

namespace {

constexpr wchar_t kWindowClassName[] = L"GrowClientWinMainClass";
constexpr wchar_t kWindowTitle[] = L"GrowClientWin - Login MVP";

constexpr int kIdEditServer = 1001;
constexpr int kIdEditGrowId = 1002;
constexpr int kIdEditPassword = 1003;
constexpr int kIdButtonLogin = 1004;
constexpr int kIdLabelStatus = 1005;

HWND g_hEditServer = nullptr;
HWND g_hEditGrowId = nullptr;
HWND g_hEditPassword = nullptr;
HWND g_hStatus = nullptr;

std::wstring GetText(HWND hWnd)
{
    int length = GetWindowTextLengthW(hWnd);
    std::wstring text;
    text.resize(static_cast<size_t>(length));
    if(length > 0) {
        GetWindowTextW(hWnd, text.data(), length + 1);
    }
    return text;
}

void SetStatus(const std::wstring& message)
{
    if(g_hStatus) {
        SetWindowTextW(g_hStatus, message.c_str());
    }
}

void HandleLogin(HWND hWnd)
{
    const std::wstring server = GetText(g_hEditServer);
    const std::wstring growId = GetText(g_hEditGrowId);
    const std::wstring password = GetText(g_hEditPassword);

    if(server.empty()) {
        SetStatus(L"Please enter server host.");
        MessageBoxW(hWnd, L"Server host is required.", L"Login", MB_OK | MB_ICONWARNING);
        return;
    }

    if(growId.empty()) {
        SetStatus(L"Please enter GrowID.");
        MessageBoxW(hWnd, L"GrowID is required.", L"Login", MB_OK | MB_ICONWARNING);
        return;
    }

    if(password.empty()) {
        SetStatus(L"Please enter password.");
        MessageBoxW(hWnd, L"Password is required.", L"Login", MB_OK | MB_ICONWARNING);
        return;
    }

    SetStatus(L"Login payload prepared. Network handshake comes next milestone.");

    std::wstring summary = L"MVP login accepted for GrowID: " + growId + L"\nServer: " + server;
    MessageBoxW(hWnd, summary.c_str(), L"GrowClientWin", MB_OK | MB_ICONINFORMATION);
}

} // namespace

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message) {
        case WM_CREATE: {
            CreateWindowW(L"STATIC", L"Server:", WS_VISIBLE | WS_CHILD,
                20, 20, 120, 20, hWnd, nullptr, nullptr, nullptr);
            g_hEditServer = CreateWindowW(L"EDIT", L"127.0.0.1",
                WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                20, 42, 340, 24, hWnd, reinterpret_cast<HMENU>(kIdEditServer), nullptr, nullptr);

            CreateWindowW(L"STATIC", L"GrowID:", WS_VISIBLE | WS_CHILD,
                20, 78, 120, 20, hWnd, nullptr, nullptr, nullptr);
            g_hEditGrowId = CreateWindowW(L"EDIT", L"",
                WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                20, 100, 340, 24, hWnd, reinterpret_cast<HMENU>(kIdEditGrowId), nullptr, nullptr);

            CreateWindowW(L"STATIC", L"Password:", WS_VISIBLE | WS_CHILD,
                20, 136, 120, 20, hWnd, nullptr, nullptr, nullptr);
            g_hEditPassword = CreateWindowW(L"EDIT", L"",
                WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                20, 158, 340, 24, hWnd, reinterpret_cast<HMENU>(kIdEditPassword), nullptr, nullptr);
            SendMessageW(g_hEditPassword, EM_SETPASSWORDCHAR, static_cast<WPARAM>(L'*'), 0);

            CreateWindowW(L"BUTTON", L"Login",
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                20, 196, 120, 32, hWnd, reinterpret_cast<HMENU>(kIdButtonLogin), nullptr, nullptr);

            g_hStatus = CreateWindowW(L"STATIC", L"Ready.",
                WS_VISIBLE | WS_CHILD,
                20, 240, 500, 20, hWnd, reinterpret_cast<HMENU>(kIdLabelStatus), nullptr, nullptr);
            return 0;
        }
        case WM_COMMAND: {
            const int controlId = LOWORD(wParam);
            const int eventCode = HIWORD(wParam);
            if(controlId == kIdButtonLogin && eventCode == BN_CLICKED) {
                HandleLogin(hWnd);
                return 0;
            }
            break;
        }
        case WM_DESTROY:
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
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kWindowClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if(!RegisterClassW(&wc)) {
        MessageBoxW(nullptr, L"Failed to register window class.", L"GrowClientWin", MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND hWnd = CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        420,
        320,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if(!hWnd) {
        MessageBoxW(nullptr, L"Failed to create main window.", L"GrowClientWin", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hWnd, nCmdShow);

    MSG msg{};
    while(GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
