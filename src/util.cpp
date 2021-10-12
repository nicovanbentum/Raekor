#include "pch.h"
#include "util.h"

namespace Raekor {

void printProgressBar(float fract) {
    static std::string bar = "----------]";
    std::string loading = "Loading textures: [";
    int index = int(fract * 10);
    if (index == 0) bar = "----------]";
    if (bar[index] == '-') bar[index] = '#';
    std::cout << '\r' << loading << bar;
    if (index == 10) std::cout << '\n';
}

//////////////////////////////////////////////////////////////////////////////////////////////////

FileWatcher::FileWatcher(const std::string& path) : path(path) {
    last_write_time = std::filesystem::last_write_time(path);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

bool FileWatcher::wasModified() {
    if (auto new_time = std::filesystem::last_write_time(path); new_time != last_write_time) {
        last_write_time = new_time;
        return true;
    }
    return false;
}

} // raekor
