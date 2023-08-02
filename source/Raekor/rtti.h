#pragma once

#include "json.h"

namespace Raekor {

class Member;

class RTTI {
    friend class RTTIFactory;

    using CreateFn    = void(*)(RTTI& rtti);
    using Constructor = void*(*)();

public:
    RTTI(const char* inName, CreateFn inCreateFn, Constructor inConstructor);
    RTTI(RTTI&) = delete;
    RTTI(RTTI&&) = delete;
    

    void            AddMember(Member* inName);
    Member*         GetMember(uint32_t inIndex) const;
    Member*         GetMember(const char* inName) const; // O(n) Expensive!
    int32_t         GetMemberIndex(const char* inName) const; // O(n) expensive!
    uint32_t        GetMemberCount() const { return uint32_t(m_Members.size()); }


    bool operator==(const RTTI& rhs) const { return m_Hash == rhs.m_Hash; }
    bool operator!=(const RTTI& rhs) const { return m_Hash != rhs.m_Hash; }

    uint32_t    GetHash() const     { return m_Hash; }
    const char* GetTypeName() const { return m_Name.c_str(); }

    inline const auto end() const   { return m_Members.end();   }
    inline const auto begin() const { return m_Members.begin(); }

private:
    uint32_t m_Hash;
    std::string m_Name;
    Constructor m_Constructor;
    std::vector<std::unique_ptr<Member>> m_Members;
};


class Member {
public:
    Member() = delete;
    Member(const char* inName, const char* inCustomName, ESerializeType inSerializeType = SERIALIZE_JSON);

    const char*     GetName() const { return m_Name; }
    uint32_t        GetNameHash() const { return m_NameHash; }
    const char*     GetCustomName() const { return m_CustomName; }
    uint32_t        GetCustomNameHash() const { return m_CustomNameHash; }

    template<typename T>
    T&              GetRef(void* inClass) { return *static_cast<T*>(GetPtr(inClass)); }
    virtual void*   GetPtr(void* inClass) = 0;

    virtual void     ToJSON(std::string& inJSON, void* inClass) { }
    virtual uint32_t FromJSON(JSON::JSONData& inJSON, uint32_t inTokenIdx, void* inClass) { return 0; }

protected:
    uint32_t m_NameHash;;
    uint32_t m_CustomNameHash;
    const char* m_Name;
    const char* m_CustomName;
    ESerializeType m_SerializeType;
};


template<typename Class, typename T>
class ClassMember : public Member {
public:
    ClassMember() = delete;
    ClassMember(const char* inName, const char* inCustomName, T Class::* inMember, ESerializeType inSerializeType = SERIALIZE_JSON) 
        : Member(inName, inCustomName, inSerializeType), m_Member(inMember) {}
    
    void ToJSON(std::string& inJSON, void* inInstance) override {
        inJSON += std::string("\"" + std::string(m_CustomName) + "\": ");
        JSON::GetValueToJSON(inJSON, GetRef<T>(inInstance));
    }

    uint32_t FromJSON(JSON::JSONData& inJSON, uint32_t inTokenIdx, void* inInstance) override {
        return inJSON.GetTokenToValue(inTokenIdx, GetRef<T>(inInstance));
    }

    virtual void* GetPtr(void* inClass) override { return &(static_cast<Class*>(inClass)->*m_Member); }

private:
    T Class::*m_Member;
};


class EnumMember : public Member {
public:
    EnumMember(const char* inName, const char* inCustomName, ESerializeType inSerializeType = SERIALIZE_JSON) 
        : Member(inName, inCustomName, inSerializeType) {}

    virtual void* GetPtr(void* inClass) { return inClass; }
};


class RTTIFactory {
public:
    static void Register(RTTI& inRTTI);

    static RTTI* GetRTTI(uint32_t inHash);
    static RTTI* GetRTTI(const char* inType);

    static void* Construct(const char* inType);

    static auto GetAllTypesIter() { return global->m_RegisteredTypes; }

private:
    std::unordered_map<uint32_t, RTTI*> m_RegisteredTypes;

    // global singleton
    static inline std::unique_ptr<RTTIFactory> global = std::make_unique<RTTIFactory>();
};

} // namespace Raekor


namespace std {
    template <> struct hash<Raekor::RTTI> {
        size_t operator()(const Raekor::RTTI& x) const {
            return size_t(x.GetHash());
        }
    };
}



#define RTTI_ENUM_HEADER(name)                                  \
RTTI& sGetRTTI(name* inName);                                   \
namespace name##Namespace { void sImplRTTI(RTTI& inRTTI); };    \


#define RTTI_ENUM_CPP(name)                                                                              \
    RTTI& sGetRTTI(name* inName) {                                                                       \
        static auto rtti = RTTI(#name, &name##Namespace::sImplRTTI, []() -> void* { return new name; }); \
        return rtti;                                                                                     \
    }                                                                                                    \
    void EShaderProgramTypeNamespace::sImplRTTI(RTTI& inRTTI)                                            \


#define RTTI_ENUM_MEMBER_CPP(enum_name, serial_type, custom_name, member_name) \
    inRTTI.AddMember(new EnumMember(#member_name, #custom_name, serial_type))

#define RTTI_ENUM_STRING_CONVERSIONS(enum_name)                              \
    inline enum_name gFromString(const char* inString) {                     \
        auto& rtti = RTTI_OF(enum_name);                                     \
        const auto index = rtti.GetMemberIndex(inString);                    \
        return index != -1 ? (enum_name)index : (enum_name)0;                \
    }                                                                        \
                                                                             \
inline const char* gToString(enum_name inType) {                             \
    auto& rtti = RTTI_OF(enum_name);                                         \
    return rtti.GetMember((uint32_t)inType)->GetName();                      \
}                                                                            \


#define RTTI_CLASS_HEADER(name)                                                                                         \
public:                                                                                                                 \
    virtual RTTI& GetRTTI();                                                                                            \
    friend RTTI& sGetRTTI(name* inName);                                                                                \
    static void sImplRTTI(RTTI& inRTTI)            


#define RTTI_CLASS_CPP(name)                                                                                            \
    RTTI& sGetRTTI(name* inName) {                                                                                      \
        static auto rtti = RTTI(#name, &name::sImplRTTI, []() -> void* { return new name; });                           \
        return rtti;                                                                                                    \
    }                                                                                                                   \
                                                                                                                        \
    RTTI& name::GetRTTI() {                                                                                             \
        return sGetRTTI(this);                                                                                          \
    }                                                                                                                   \
                                                                                                                        \
    void name::sImplRTTI(RTTI& inRTTI)    


#define RTTI_CLASS_CPP_NO_FACTORY(name)                                                                                 \
    RTTI& sGetRTTI(name* inName) {                                                                                      \
        static auto rtti = RTTI(#name, &name::sImplRTTI, []() -> void* { return nullptr; });                            \
        return rtti;                                                                                                    \
    }                                                                                                                   \
                                                                                                                        \
    RTTI& name::GetRTTI() {                                                                                             \
        return sGetRTTI(this);                                                                                          \
    }                                                                                                                   \
                                                                                                                        \
    void name::sImplRTTI(RTTI& inRTTI)   


#define RTTI_MEMBER_CPP(class_name, serial_type, custom_name, member_name)                                              \
    inRTTI.AddMember(new ClassMember<class_name, decltype(class_name::member_name)>(#member_name, custom_name, &class_name::member_name, serial_type))


#define RTTI_OF(name) sGetRTTI((name*)nullptr)

template<typename T>
Raekor::RTTI& gGetRTTI() { return sGetRTTI((T*)nullptr); }

