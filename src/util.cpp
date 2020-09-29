#include "pch.h"
#include "util.h"

namespace Raekor {

Stb::Image::Image(uint32_t format, const std::string& fp) : filepath(fp), format(format) {}

//////////////////////////////////////////////////////////////////////////////////////////////////

Stb::Image::~Image() { if (pixels != nullptr)  stbi_image_free(pixels); }

//////////////////////////////////////////////////////////////////////////////////////////////////

void Stb::Image::load(const std::string& fp, bool loadFlipped) {
    if (!std::filesystem::exists(fp)) {
        std::clog << "Image file \"" << fp << "\" not found on disk.\n";
    }
    this->filepath = fp;
    stbi_set_flip_vertically_on_load(loadFlipped);
    pixels = stbi_load(fp.c_str(), &w, &h, &channels, static_cast<uint32_t>(format));
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void printProgressBar(int val, int min, int max) {
    static std::string bar = "----------]";
    std::string loading = "Loading textures: [";
    auto index = (val - min) * (10 - 0) / (max - min) + 0;
    if (index == 0) bar = "----------]";
    if (bar[index] == '-') bar[index] = '#';
    std::cout << '\r' << loading << bar;
    if (val == max) std::cout << std::endl;
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
