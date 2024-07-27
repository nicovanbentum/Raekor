#pragma once

#include "PCH.h"

namespace RK {

template<typename T>
class Slice
{
public:
    explicit Slice() : start(nullptr), length(0) {}

    Slice(const T* inStart) : start(inStart), length(1) {}
    Slice(const T* inStart, uint64_t inLength) : start(inStart), length(inLength) {}
    Slice(const T* inStart, const T* inEnd) : start(inStart), length(inEnd - inStart) {}

    template<unsigned int N>
    Slice(const std::array<T, N>& inArray) : start(inArray.data()), length(inArray.size()) {}
    Slice(const std::vector<T>& inVector) : start(inVector.data()), length(inVector.size()) {}
    Slice(const std::string& inString) : start(inString.c_str()), length(inString.size()) {}
    Slice(std::initializer_list<T> inInitList) : start(inInitList.begin()), length(inInitList.size()) {}

    using value_type = T;
    using reference = T&;

    struct iterator
    {
        using reference = T&;
        using value_type = T;

        uint64_t length = 0;
        const T* start = nullptr;

        bool operator != (const iterator& other) const { return start != other.start; }
        bool operator == (const iterator& other) const { return start == other.start; }

        void operator ++ () { ++start; }
        auto operator *() const { return *start; }
    };

    using const_iterator = iterator;

    const T& operator[](size_t index) const 
    {
        const T* ptr = start + index;
        assert(ptr != nullptr && ptr < start + length);
        return *ptr;
    }

    const T* begin() { return start; }
    const T* end() { return ( start + length ); }
    const T* begin() const { return start; }
    const T* end() const { return ( start + length ); }

    bool IsEmpty() const { return start == nullptr || length == 0; }
    uint64_t Length() const { return length; }
    uint64_t SizeInBytes() const { return length * sizeof(T); }

    const T* GetPtr() const { return start; }

    Slice<T> Sub(size_t inOffset, size_t inCount) { return Slice(start + inOffset, inCount); }

private:
    const T* start = nullptr;
    uint64_t length = 0;
};



template<typename T>
class MutSlice
{
public:
    explicit MutSlice() : start(nullptr), length(0) {}

    MutSlice(T* inStart) : start(inStart), length(1) {}
    MutSlice(T* inStart, uint64_t inLength) : start(inStart), length(inLength) {}
    MutSlice(T* inStart, T* inEnd) : start(inStart), length(inEnd - inStart) {}

    template<unsigned int N>
    MutSlice(std::array<T, N>& inArray) : start(inArray.data()), length(inArray.size()) {}
    MutSlice(std::vector<T>& inVector) : start(inVector.data()), length(inVector.size()) {}
    MutSlice(std::string& inString) : start(inString.c_str()), length(inString.size()) {}
    MutSlice(std::initializer_list<T> inInitList) : start(inInitList.begin()), length(inInitList.size()) {}

    using value_type = T;
    using reference = T&;

    struct iterator
    {
        using reference = T&;
        using value_type = T;

        uint64_t length = 0;
        T* start = nullptr;

        bool operator != (iterator& other) { return start != other.start; }
        bool operator == (iterator& other) { return start == other.start; }

        void operator ++ () { ++start; }
        auto operator *() { return *start; }
    };

    using const_iterator = iterator;

    T& operator[](size_t index)
    {
        T* ptr = start + index;
        assert(ptr != nullptr && ptr < start + length);
        return *ptr;
    }

    const T* begin() { return start; }
    const T* end() { return ( start + length ); }
    const T* begin() const { return start; }
    const T* end() const { return ( start + length ); }

    bool IsEmpty() const { return start == nullptr || length == 0; }
    uint64_t Length() const { return length; }
    uint64_t SizeInBytes() const { return length * sizeof(T); }

    T* GetPtr() { return start; }

    MutSlice<T> Sub(size_t inOffset, size_t inCount) { return MutSlice(start + inOffset, inCount); }

private:
    T* start = nullptr;
    uint64_t length = 0;
};


using ByteSlice = Slice<uint8_t>;
using ByteMutSlice = MutSlice<uint8_t>;

} // namespace::Raekor