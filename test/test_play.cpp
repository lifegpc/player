#include <windows.h>
#include "../player.h"
#include <string>
#include "wchar_util.h"

// Function to open a file dialog and get the selected file path
std::string GetOpenFileName() {
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
    set_player_log_file("test_play.log", 0, AV_LOG_DEBUG);
    while (true) {
        std::string path = GetOpenFileName();
        if (path.empty()) {
            break;
        }
        play(path.c_str());
    }
}
