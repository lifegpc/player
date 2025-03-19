#include "fileop.h"
#include "wchar_util.h"
#include "fcntl.h"

bool convertFileToUtf16(std::string path) {
    size_t size = 0;
    if (!fileop::get_file_size(path, size)) {
        return false;
    }
    FILE* f = fileop::fopen(path, "rb");
    if (!f) {
        return false;
    }
    char* buf = new char[size];
    if (!buf) {
        fclose(f);
        return false;
    }
    if (fread(buf, 1, size, f) != size) {
        delete[] buf;
        fclose(f);
        return false;
    }
    fclose(f);
    std::wstring wc;
    if (!wchar_util::str_to_wstr(wc, buf, 65001)) {
        delete[] buf;
        return false;
    }
    delete[] buf;
    f = fileop::fopen(path, "wb");
    if (!f) {
        return false;
    }
    // 写入 BOM
    if (fwrite(L"\uFEFF", 2, 1, f) != 1) {
        fclose(f);
        return false;
    }
    if (fwrite(wc.c_str(), 2, wc.size(), f) != wc.size()) {
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
}

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        return 1;
    }
    bool isWide = false;
    int wargc = 0;
    char** wargv;
    if (wchar_util::getArgv(wargv, wargc)) {
        isWide = true;
        argc = wargc;
        argv = (const char**)wargv;
    }
    for (int i = 1; i < argc; ++i) {
        if (!convertFileToUtf16(argv[i])) {
            printf("Convert failed.");
            return 1;
        }
    }
    return 0;
}
