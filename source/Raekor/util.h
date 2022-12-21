#pragma once

#ifndef NDEBUG
    #define m_assert(expr, msg) if(!expr) std::cout << msg << std::endl; assert(expr);
#else 
    #define m_assert(expr, msg)
#endif

#define ALIGN(x) __declspec(align(x))

#define COUT_NC "\033[0m"
#define COUT_RED(str) "\033[0;31m" str COUT_NC
#define COUT_GREEN(str) "\033[1;32m" str COUT_NC

#define LOG_CATCH(code) try { code; } catch(std::exception e) { std::cout << e.what() << '\n'; }

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
    return ImVec2(float(vec.x), float(vec.y));
}

template<typename T>
constexpr ImVec4 ImVec(const glm::vec<4, T>& vec) {
    return ImVec4(float(vec.x), float(vec.y), float(vec.z), float(vec.w));
}

inline ImVec2 operator*(const ImVec2& lhs, const ImVec2& rhs) {
    return ImVec2(lhs.x * rhs.x, lhs.y * rhs.y);
}

template<typename T>
inline JPH::Vec3 FromGLM(const glm::vec<3, T>& vec) {
    return JPH::Vec3(float(vec.x), float(vec.y), float(vec.z));
}

inline JPH::Quat FromGLM(const glm::quat& quat) {
    return JPH::Quat(float(quat.x), float(quat.y), float(quat.z), float(quat.w));
}

// std::string version of https://docs.microsoft.com/en-us/cpp/text/how-to-convert-between-various-string-types?view=msvc-160
inline std::string gWCharToString(wchar_t* wchars) {
    const auto len = wcslen(wchars);
    auto string = std::string((len + 1) * 2, 0);

    size_t convertedChars = 0;
    wcstombs_s(&convertedChars, string.data(), string.size(), wchars, _TRUNCATE);

    return string.substr(0, len);
}


template<typename T>
class Slice {
public:
    Slice() = delete;
    Slice(const T* start, uint64_t length) : start(start), length(length) {}
    Slice(const T* start, const T* end) : start(start), length(end - start) {}

    Slice(const std::vector<T>& inVec) : start(inVec.data()), length(inVec.size()) {}

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

    Slice<T> Sub(size_t inOffset, size_t inCount) { return Slice(start + inOffset, inCount); }

private:
    const T* start = nullptr;
    uint64_t length = 0;
};


template<typename T>
class RTID {
public:
    static inline uint32_t INVALID = UINT32_MAX;

    RTID() : index(INVALID) {}
    explicit RTID(uint32_t inIndex) : index(inIndex) {}
    
    bool operator==(const RTID<T>& inOther) const { return ToIndex() == inOther.ToIndex(); }
    bool operator!=(const RTID<T>& inOther) const { return ToIndex() != inOther.ToIndex(); }

    uint32_t ToIndex() const { return index; }
    [[nodiscard]] bool Isvalid() const { return index != INVALID; }

private:
    uint32_t index;
};


template<typename T>
class FreeVector {
public:
    using ID = RTID<T>;
    virtual ~FreeVector() = default;

    [[nodiscard]] virtual ID Add(const T& inT) {
        size_t t_index;

        if (m_FreeIndices.empty()) {
            m_Storage.emplace_back(inT);
            t_index = m_Storage.size() - 1;
        }
        else {
            t_index = m_FreeIndices.back();
            m_Storage[t_index] = inT;
            m_FreeIndices.pop_back();
        }

        return ID(t_index);
    }

    bool Remove(ID inID) {
        if (inID.ToIndex() > m_Storage.size() - 1)
            return false;

        m_FreeIndices.push_back(inID.ToIndex());
        return true;
    }

    T& Get(ID inRTID) {
        assert(!m_Storage.empty() && inRTID.ToIndex() < m_Storage.size());
        return m_Storage[inRTID.ToIndex()];
    }

    const T& Get(ID inRTID) const {
        assert(!m_Storage.empty() && inRTID.ToIndex() < m_Storage.size());
        return m_Storage[inRTID.ToIndex()];
    }

private:
    std::vector<T> m_Storage;
    std::vector<size_t> m_FreeIndices;
};



template <typename T,
    typename TIter = decltype(std::begin(std::declval<T>())),
    typename = decltype(std::end(std::declval<T>()))>
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

template<typename T> struct std::hash<Raekor::RTID<T>> {
    std::size_t operator()(const Raekor::RTID<T>& inID) const noexcept {
        return size_t(inID.ToIndex());
    }
};
