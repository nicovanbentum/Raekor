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

constexpr inline size_t align_up(size_t value, size_t alignment) noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}



void printProgressBar(float fract);



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




class FileWatcher {
public:
    FileWatcher(const std::string& path);

    bool wasModified();

private:
    std::string path;
    std::filesystem::file_time_type last_write_time;
};



template <typename T>
constexpr ImVec2 ImVec(const glm::vec<2, T>& vec) {
    return ImVec2(static_cast<float>(vec.x), static_cast<float>(vec.y));
}

inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) {
    return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
}

inline ImVec2 operator*(const ImVec2& lhs, const ImVec2& rhs) {
    return ImVec2(lhs.x * rhs.x, lhs.y * rhs.y);
}

inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) {
    return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}



// std::string version of https://docs.microsoft.com/en-us/cpp/text/how-to-convert-between-various-string-types?view=msvc-160
inline std::string wchar_to_std_string(wchar_t* wchars) {
    const auto len = wcslen(wchars);
    auto string = std::string((len + 1) * 2, 0);

    size_t convertedChars = 0;
    wcstombs_s(&convertedChars, string.data(), string.size(), wchars, _TRUNCATE);

    return string.substr(0, len);
}

} // Namespace Raekor
