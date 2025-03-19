#include <windows.h>
#include "../player.h"
#include <string>
#include "wchar_util.h"

// Function to open a file dialog and get the selected file path
std::string MyGetOpenFileName() {
    wchar_t path[MAX_PATH] = L"";
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = L"All Files (*.*)\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"";
    
    if (GetOpenFileNameW(&ofn)) {
        std::string tmp;
        if (!wchar_util::wstr_to_str(tmp, ofn.lpstrFile, CP_UTF8)) {
            return "";
        }
        return tmp;
    }
    
    return "";
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    set_player_log_file("test_play_from_hwnd.log", 0, AV_LOG_DEBUG);
    // Create a window
    HWND hwnd;
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"PlayerWindow";

    RegisterClassW(&wc);

    // Create the main window
    hwnd = CreateWindowW(
        L"PlayerWindow",
        L"Video Player",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        MessageBoxW(NULL, L"Window Creation Failed", L"Error", MB_ICONERROR);
        return 0;
    }

    // Show the window
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    std::string path = MyGetOpenFileName();
        if (path.empty()) {
            return 0;
        }
        play(path.c_str(), (void**)&hwnd);
    // Message loop to handle window events including WM_PAINT (exposure)
    MSG msg;
    BOOL bRet;

    // Define a message handling function
    auto handleMessages = [&]() {
        while (GetMessage(&msg, NULL, 0, 0)) {
            if (msg.message == WM_QUIT) {
                PostQuitMessage(0);
                return false;
            }
            if (msg.message == WM_PAINT) {
                return true;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            
        }
        return true;
    };

    // Initialize message loop
    handleMessages();
}
