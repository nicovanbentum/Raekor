#pragma once

// code to literal, for when I don't feel thinking of a better literal string
#define nameof(var) (#var)

// message assert macro
#ifndef NDEBUG
    #define m_assert(expr, msg) if(!expr) std::cout << msg << std::endl; assert(expr);
#else 
    #define m_assert(expr, msg)
#endif


// alias for Microsoft's com pointers
#ifdef _WIN32
template<typename T>
using com_ptr = Microsoft::WRL::ComPtr<T>;
#endif

#define LOG_CATCH(code) try { code; } catch(std::exception e) { std::cout << e.what() << '\n'; }

namespace Raekor {

enum class PATH_OPTIONS {
    DIR,
    FILENAME,
    EXTENSION,
    FILENAME_AND_EXTENSION
};

static std::string parseFilepath(const std::string& path, PATH_OPTIONS option) {
    std::ifstream valid(path);
    m_assert(valid, "invalid path string passed");
    // find the last slash character index
    auto backslash = path.rfind('\\');
    auto fwdslash = path.rfind('/');
    size_t lastSlash;
    if (backslash == std::string::npos) lastSlash = fwdslash;
    else if (fwdslash == std::string::npos) lastSlash = backslash;
    else lastSlash = std::max(backslash, fwdslash);

    auto dot = path.find('.');

    switch(option) {
    case PATH_OPTIONS::DIR: return path.substr(0, lastSlash +1);
    case PATH_OPTIONS::FILENAME: return path.substr(lastSlash +1, (dot - lastSlash) - 1);
    case PATH_OPTIONS::EXTENSION: return path.substr(dot);
    case PATH_OPTIONS::FILENAME_AND_EXTENSION: return path.substr(lastSlash +1);
    default: return {};
    }
}

static void printProgressBar(int val, int min, int max) {
    static std::string bar = "----------]";
    std::string loading = "Loading textures: [";
    auto index = (val - min) * (10 - 0) / (max - min) + 0;
    if (index == 0) bar = "----------]";
    if (bar[index] == '-') bar[index] = '#';
    std::cout << '\r' << loading << bar;
    if (val == max) std::cout << std::endl;
};

enum { RGB = 3, RGBA = 4 };

namespace Stb {
    struct Image {
        Image(uint32_t format = RGBA) : filepath(""), format(format) {}
        ~Image() { if (pixels != nullptr)  stbi_image_free(pixels); }

        void load(const std::string& fp, bool loadFlipped = false) {
            this->filepath = fp;
            stbi_set_flip_vertically_on_load(loadFlipped);
            pixels = stbi_load(fp.c_str(), &w, &h, &channels, static_cast<uint32_t>(format));
        }

        uint32_t format;
        int w, h, channels;
        bool isSRGB = false;
        std::string filepath;
        unsigned char* pixels = nullptr;
    };
}

class FileWatcher {
public:
    FileWatcher(const std::string& path) : path(path) {
        last_write_time = std::filesystem::last_write_time(path);
    }

    bool wasModified() {
        if (auto new_time = std::filesystem::last_write_time(path); new_time != last_write_time) {
            last_write_time = new_time;
            return true;
        }
        return false;
    }

private:
    std::string path;
    std::filesystem::file_time_type last_write_time;
};


} // Namespace Raekor
