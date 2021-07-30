#pragma once

// code to literal, for when I don't feel thinking of a better literal string
#define nameof(var) (#var)

//////////////////////////////////////////////////////////////////////////////////////////////////

// message assert macro
#ifndef NDEBUG
    #define m_assert(expr, msg) if(!expr) std::cout << msg << std::endl; assert(expr);
#else 
    #define m_assert(expr, msg)
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////

// alias for Microsoft's com pointers
#ifdef _WIN32
template<typename T>
using com_ptr = Microsoft::WRL::ComPtr<T>;
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////

#define LOG_CATCH(code) try { code; } catch(std::exception e) { std::cout << e.what() << '\n'; }

//////////////////////////////////////////////////////////////////////////////////////////////////

namespace Raekor {

enum class PATH_OPTIONS {
    DIR,
    FILENAME,
    EXTENSION,
    FILENAME_AND_EXTENSION
};

//////////////////////////////////////////////////////////////////////////////////////////////////

void printProgressBar(int val, int min, int max);;

//////////////////////////////////////////////////////////////////////////////////////////////////

enum { RGB = 3, RGBA = 4 };

//////////////////////////////////////////////////////////////////////////////////////////////////

namespace Stb {
    struct Image {
        Image(uint32_t format = RGBA, const std::string& fp = "");
        ~Image();

        void load(const std::string& fp, bool loadFlipped = false);

        uint32_t format;
        int w, h, channels;
        bool isSRGB = false;
        std::string filepath;
        unsigned char* pixels = nullptr;
    };
}

//////////////////////////////////////////////////////////////////////////////////////////////////

template <typename Tpl, typename Fx, size_t... Indices>
void for_each_tuple_element_impl(Tpl&& Tuple, Fx Func, std::index_sequence<Indices...>) {
    // call Func() on the Indices elements of Tuple
    int ignored[] = { (static_cast<void>(Func(std::get<Indices>(std::forward<Tpl>(Tuple)))), 0)... };
    (void)ignored;
}

template <typename Tpl, typename Fx>
void for_each_tuple_element(Tpl&& Tuple, Fx Func) { // call Func() on each element in Tuple
    for_each_tuple_element_impl(
        std::forward<Tpl>(Tuple), Func, std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<Tpl>>>{});
}

//////////////////////////////////////////////////////////////////////////////////////////////////


class FileWatcher {
public:
    FileWatcher(const std::string& path);

    bool wasModified();

private:
    std::string path;
    std::filesystem::file_time_type last_write_time;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
constexpr ImVec2 ImVec(const glm::vec<2, T>& vec) {
    return ImVec2(static_cast<float>(vec.x), static_cast<float>(vec.y));
}

inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) {
    return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
}

inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) {
    return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}


} // Namespace Raekor
