#include "pch.h"
#include "util.h"

namespace Raekor {

void gPrintProgressBar(const std::string& prepend, float fract) {
    static std::string bar = "----------]";
    std::string loading = prepend + "[";
    int index = int(fract * 10);
    if (index == 0) bar = "----------]";
    if (bar[index] == '-') bar[index] = '#';
    std::cout << '\r' << loading << bar;
    if (index == 10) std::cout << '\n';
}


FileWatcher::FileWatcher(const std::string& path) : m_Path(path) {
    m_LastWriteTime = std::filesystem::last_write_time(path);
}


bool FileWatcher::WasModified() {
    if (auto new_time = std::filesystem::last_write_time(m_Path); new_time != m_LastWriteTime) {
        m_LastWriteTime = new_time;
        return true;
    }
    return false;
}

} // raekor
