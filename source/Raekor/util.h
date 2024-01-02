#pragma once

#ifndef NDEBUG
    #define RK_ASSERT(expr, msg) if(!expr) std::cout << msg << std::endl; assert(expr);
#else 
    #define RK_ASSERT(expr, msg)
#endif

#define ALIGN(x) __declspec(align(x))

#ifndef TOKENPASTE
#define TOKENPASTE(x, y) x ## y
#endif

#ifndef TOKENPASTE2
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
#endif

#define COUT_NC "\033[0m"
#define COUT_RED(str) "\033[0;31m" str COUT_NC
#define COUT_GREEN(str) "\033[1;32m" str COUT_NC

#define LOG_CATCH(code) try { code; } catch(std::exception e) { std::cout << e.what() << '\n'; }

#ifndef PRAGMA_OPTIMIZE_OFF
#define PRAGMA_OPTIMIZE_OFF __pragma( optimize( "", off ) )
#endif

#ifndef PRAGMA_OPTIMIZE_ON
#define PRAGMA_OPTIMIZE_ON __pragma( optimize( "", on ) )
#endif

#define NO_COPY_NO_MOVE(Type) \
    Type(Type&) = delete; \
    Type(Type&&) = delete; \
    Type& operator=(Type&) = delete; \
    Type& operator=(Type&&) = delete \

#define TEXTURE_SWIZZLE_RGBA 0b11'10'01'00
#define TEXTURE_SWIZZLE_RRRR 0b00'00'00'00
#define TEXTURE_SWIZZLE_GGGG 0b01'01'01'01
#define TEXTURE_SWIZZLE_BBBB 0b10'10'10'10
#define TEXTURE_SWIZZLE_AAAA 0b11'11'11'11

inline uint8_t gUnswizzleComponent(uint8_t swizzle, uint8_t channel) { return ( ( swizzle >> (channel * 2)) & 0b00'00'00'11 ); }
inline std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> gUnswizzleComponents(uint8_t swizzle)
{ 
    return std::make_tuple
    (
        gUnswizzleComponent(swizzle, 0), 
        gUnswizzleComponent(swizzle, 1),
        gUnswizzleComponent(swizzle, 2),
        gUnswizzleComponent(swizzle, 3)
    );
}

// From https://en.cppreference.com/w/cpp/utility/unreachable
[[noreturn]] constexpr inline void gUnreachableCode() {
    assert(false);
    // Uses compiler specific extensions if possible.
    // Even if no extension is used, undefined behavior is still raised by
    // an empty function body and the noreturn attribute.
#ifdef __GNUC__ // GCC, Clang, ICC
    __builtin_unreachable();
    #elifdef _MSC_VER // MSVC
        __assume(false);
#endif
}

namespace Raekor {

template<typename T>
concept HasRTTI = requires (T t) {
    t.GetRTTI();
};

bool SDL_IsWindowBorderless(SDL_Window* inWindow);
bool SDL_IsWindowExclusiveFullscreen(SDL_Window* inWindow);

class INoCopyNoMove 
{
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


inline constexpr uint32_t gHash32Bit(const char* const str, const uint32_t value = val_32_const) noexcept 
{
    return (str[0] == '\0') ? value : gHash32Bit(&str[1], (value ^ uint32_t(str[0])) * prime_32_const);
}


inline constexpr uint64_t gHash64Bit(const char* const str, const uint64_t value = val_64_const) noexcept 
{
    return (str[0] == '\0') ? value : gHash64Bit(&str[1], (value ^ uint64_t(str[0])) * prime_64_const);
}


inline uint64_t gHashFNV1a(const char* const inData, uint64_t inLength) 
{
    auto hash = val_64_const;

    for (uint32_t i = 0; i < inLength; i++)
        hash = (hash ^ inData[i]) * prime_64_const;

    return hash;
}


constexpr inline size_t gAlignUp(size_t value, size_t alignment) noexcept 
{
    return (value + alignment - 1) & ~(alignment - 1);
}


const char* gAsciiProgressBar(float fract);


template <typename Tpl, typename Fx, size_t... Indices>
void _for_each_tuple_element_impl(Tpl&& Tuple, Fx Func, std::index_sequence<Indices...>) 
{
    // call Func() on the Indices elements of Tuple
    int ignored[] = { (static_cast<void>(Func(std::get<Indices>(std::forward<Tpl>(Tuple)))), 0)... };
    (void)ignored;
}

template <typename Tpl, typename Fx>
void gForEachTupleElement(Tpl&& Tuple, Fx Func) 
{ 
    // call Func() on each element in Tuple
    _for_each_tuple_element_impl(
        std::forward<Tpl>(Tuple), Func, std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<Tpl>>>{});
}


class FileWatcher {
public:
    FileWatcher(const std::string& path);

    bool WasModified();

private:
    Path m_Path;
    std::filesystem::file_time_type m_LastWriteTime;
};


template <typename T>
constexpr ImVec2 ImVec(const glm::vec<2, T>& vec) 
{
    return ImVec2(float(vec.x), float(vec.y));
}

template<typename T>
constexpr ImVec4 ImVec(const glm::vec<4, T>& vec) 
{
    return ImVec4(float(vec.x), float(vec.y), float(vec.z), float(vec.w));
}

inline ImVec2 operator*(const ImVec2& lhs, const ImVec2& rhs) 
{
    return ImVec2(lhs.x * rhs.x, lhs.y * rhs.y);
}

template<typename T>
inline JPH::Vec3 FromGLM(const glm::vec<3, T>& vec) 
{
    return JPH::Vec3(float(vec.x), float(vec.y), float(vec.z));
}

inline JPH::Quat FromGLM(const glm::quat& quat) 
{
    return JPH::Quat(float(quat.x), float(quat.y), float(quat.z), float(quat.w));
}

// std::string version of https://docs.microsoft.com/en-us/cpp/text/how-to-convert-between-various-string-types?view=msvc-160
inline std::string gWCharToString(const wchar_t* wchars) 
{
    const auto len = wcslen(wchars);
    auto string = std::string((len + 1) * 2, 0);

    size_t convertedChars = 0;
    wcstombs_s(&convertedChars, string.data(), string.size(), wchars, _TRUNCATE);

    return string.substr(0, len);
}


template<typename T>
class Slice 
{
public:
    Slice() = delete;
    Slice(const T* start) : start(start), length(1) {}
    Slice(const T* start, uint64_t length) : start(start), length(length) {}
    Slice(const T* start, const T* end) : start(start), length(end - start) {}
    Slice(std::initializer_list<T> init_list) : start(init_list.begin()), length(init_list.size()) {}

    Slice(const std::vector<T>& inVec) : start(inVec.data()), length(inVec.size()) {}
    Slice(const std::string& inString) : start(inString.c_str()), length(inString.size()) {}

    using value_type = T;
    using reference = T&;

    struct iterator
    {
        using reference = T&;
        using value_type = T;
        
        const T* start = nullptr;
        uint64_t length = 0;
        bool operator != (const iterator& other) const { return start != other.start; }
        bool operator == (const iterator& other) const { return start == other.start; }
        void operator ++ () { ++start; }
        auto operator *() const { return *start; }
    };

    using const_iterator = iterator;

    const T& operator[](size_t index) const {
        const auto ptr = start + index;
        assert(ptr != nullptr && ptr < start + length);
        return *ptr;
    }

    auto begin() { return start; }
    auto end() { return (start + length); }
    const auto begin() const { return start; }
    const auto end() const { return (start + length); }

    bool IsEmpty() const { return start == nullptr || length == 0; }
    uint64_t Length() const { return length; }

    const T* GetPtr() const { return start; }

    Slice<T> Sub(size_t inOffset, size_t inCount) { return Slice(start + inOffset, inCount); }

private:
    const T* start = nullptr;
    uint64_t length = 0;
};




template <typename T, typename TIter = decltype(std::begin(std::declval<T>())), typename = decltype(std::end(std::declval<T>()))>
constexpr auto gEnumerate(T&& iterable)
{
    struct iterator
    {
        size_t i;
        TIter iter;
        bool operator != (const iterator& other) const { return iter != other.iter; }
        void operator ++ () { ++i; ++iter; }
        auto operator * () const { return std::tie(i, *iter); }
    };

    struct iterable_wrapper
    {
        T iterable;
        auto begin() { return iterator{ 0, std::begin(iterable) }; }
        auto end() { return iterator{ 0, std::end(iterable) }; }
    };

    return iterable_wrapper{ std::forward<T>(iterable) };
}

#define gWarn(inStr) std::cout << "Warning in File " << __FILE__ << " at Line " << __LINE__ << " from function " << __FUNCTION__ << ": " << inStr << '\n';

} // Namespace Raekor


