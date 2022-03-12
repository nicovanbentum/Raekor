#pragma once

#ifndef NDEBUG
    #define m_assert(expr, msg) if(!expr) std::cout << msg << std::endl; assert(expr);
#else 
    #define m_assert(expr, msg)
#endif


#define COUT_NC "\033[0m"
#define COUT_RED(str) "\033[0;31m" str COUT_NC
#define COUT_GREEN(str) "\033[1;32m" str COUT_NC


#define LOG_CATCH(code) try { code; } catch(std::exception e) { std::cout << e.what() << '\n'; }


namespace Raekor {

class INoCopyNoMove {
public:
    INoCopyNoMove() = default;
    ~INoCopyNoMove() = default;
    INoCopyNoMove(INoCopyNoMove&) = delete;
    INoCopyNoMove(INoCopyNoMove&&) = delete;
    INoCopyNoMove& operator=(INoCopyNoMove&) = delete;
    INoCopyNoMove& operator=(INoCopyNoMove&&) = delete;
};


constexpr uint32_t val_32_const = 0x811c9dc5;
constexpr uint32_t prime_32_const = 0x1000193;
constexpr uint64_t val_64_const = 0xcbf29ce484222325;
constexpr uint64_t prime_64_const = 0x100000001b3;


inline constexpr uint32_t gHash32Bit(const char* const str, const uint32_t value = val_32_const) noexcept {
    return (str[0] == '\0') ? value : gHash32Bit(&str[1], (value ^ uint32_t(str[0])) * prime_32_const);
}


inline constexpr uint64_t gHash64Bit(const char* const str, const uint64_t value = val_64_const) noexcept {
    return (str[0] == '\0') ? value : gHash64Bit(&str[1], (value ^ uint64_t(str[0])) * prime_64_const);
}


constexpr inline size_t gAlignUp(size_t value, size_t alignment) noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}


void gPrintProgressBar(const std::string& prepend, float fract);


template <typename Tpl, typename Fx, size_t... Indices>
void _for_each_tuple_element_impl(Tpl&& Tuple, Fx Func, std::index_sequence<Indices...>) {
    // call Func() on the Indices elements of Tuple
    int ignored[] = { (static_cast<void>(Func(std::get<Indices>(std::forward<Tpl>(Tuple)))), 0)... };
    (void)ignored;
}

template <typename Tpl, typename Fx>
void gForEachTupleElement(Tpl&& Tuple, Fx Func) { // call Func() on each element in Tuple
    _for_each_tuple_element_impl(
        std::forward<Tpl>(Tuple), Func, std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<Tpl>>>{});
}


class FileWatcher {
public:
    FileWatcher(const std::string& path);

    bool WasModified();

private:
    std::string m_Path;
    std::filesystem::file_time_type m_LastWriteTime;
};


template <typename T>
constexpr ImVec2 ImVec(const glm::vec<2, T>& vec) {
    return ImVec2(static_cast<float>(vec.x), static_cast<float>(vec.y));
}

template<typename T>
constexpr ImVec4 ImVec(const glm::vec<4, T>& vec) {
    return ImVec4(static_cast<float>(vec.x), static_cast<float>(vec.y), static_cast<float>(vec.z), static_cast<float>(vec.w));
}

inline ImVec2 operator*(const ImVec2& lhs, const ImVec2& rhs) {
    return ImVec2(lhs.x * rhs.x, lhs.y * rhs.y);
}

template<typename T>
inline JPH::Vec3 FromGLM(const glm::vec<3, T>& vec) {
    return JPH::Vec3(static_cast<float>(vec.x), static_cast<float>(vec.y), static_cast<float>(vec.z));
}

inline JPH::Quat FromGLM(const glm::quat& quat) {
    return JPH::Quat(static_cast<float>(quat.x), static_cast<float>(quat.y), static_cast<float>(quat.z), static_cast<float>(quat.w));
}

// std::string version of https://docs.microsoft.com/en-us/cpp/text/how-to-convert-between-various-string-types?view=msvc-160
inline std::string gWCharToString(wchar_t* wchars) {
    const auto len = wcslen(wchars);
    auto string = std::string((len + 1) * 2, 0);

    size_t convertedChars = 0;
    wcstombs_s(&convertedChars, string.data(), string.size(), wchars, _TRUNCATE);

    return string.substr(0, len);
}

} // Namespace Raekor
