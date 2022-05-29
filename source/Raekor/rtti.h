#pragma once

namespace Raekor {

class RTTI {
public:
    RTTI(const char* name);

    bool operator==(const RTTI& rhs) const { return m_NameHash == rhs.m_NameHash; }
    bool operator!=(const RTTI& rhs) const { return m_NameHash != rhs.m_NameHash; }

    uint32_t GetHash() const { return m_NameHash; }

private:
    uint32_t m_NameHash;
};


} // namespace Raekor

namespace std {
    template <> struct hash<Raekor::RTTI> {
        size_t operator()(const Raekor::RTTI& x) const {
            return static_cast<size_t>(x.GetHash());
        }
    };
}

#define DECLARE_RTTI(name)                  \
    static RTTI& sGetRTTI() {               \
        static auto rtti = RTTI(#name);     \
        return rtti;                        \
    }  