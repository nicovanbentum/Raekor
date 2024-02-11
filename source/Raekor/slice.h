#pragma once

namespace Raekor {

template<typename T>
class Slice
{
public:
    Slice() = delete;

    Slice(const T* inStart) : start(inStart), length(1) {}
    Slice(const T* inStart, uint64_t inLength) : start(inStart), length(inLength) {}
    Slice(const T* inStart, const T* inEnd) : start(inStart), length(inEnd - inStart) {}

    Slice(const std::vector<T>& inVec) : start(inVec.data()), length(inVec.size()) {}
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
        const auto ptr = start + index;
        assert(ptr != nullptr && ptr < start + length);
        return *ptr;
    }

    auto begin() { return start; }
    auto end() { return ( start + length ); }
    const auto begin() const { return start; }
    const auto end() const { return ( start + length ); }

    bool IsEmpty() const { return start == nullptr || length == 0; }
    uint64_t Length() const { return length; }
    uint64_t SizeInBytes() const { return length * sizeof(T); }

    const T* GetPtr() const { return start; }

    Slice<T> Sub(size_t inOffset, size_t inCount) { return Slice(start + inOffset, inCount); }

private:
    const T* start = nullptr;
    uint64_t length = 0;
};

} // namespace::Raekor