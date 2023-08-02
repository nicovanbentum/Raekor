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
    Member*         GetMember(const std::string& inName) const; // Expensive!
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
    Member(const char* inName, const char* inCustomName, ESerializeType inSerializeType = SERIALIZE_JSON) 
        : m_Name(inName), m_CustomName(inCustomName) {}

    const char*     GetName() const { return m_Name; }
    const char*     GetCustomName() const { return m_CustomName; }
    virtual void*   GetMember(void* inClass) = 0;

    template<typename T>
    T& GetMemberRef(void* inClass) { return *static_cast<T*>(GetMember(inClass)); }

    virtual void     ToJSON(std::string& inJSON, void* inClass) { }
    virtual uint32_t FromJSON(JSON::JSONData& inJSON, uint32_t inTokenIdx, void* inClass) { return 0; }

    virtual bool IsPtr() { return false; }
    virtual void SetPtr() {}

protected:
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
    
    virtual bool IsPtr() { return std::is_pointer_v<T>; }

    void ToJSON(std::string& inJSON, void* inInstance) override {
        inJSON += std::string("\"" + std::string(m_CustomName) + "\": ");
        JSON::GetValueToJSON(inJSON, *static_cast<T*>(GetMember(inInstance)));
    }

    uint32_t FromJSON(JSON::JSONData& inJSON, uint32_t inTokenIdx, void* inInstance) override {
        return inJSON.GetTokenToValue(inTokenIdx, *static_cast<T*>(GetMember(inInstance)));
    }

    virtual void* GetMember(void* inClass) override { return &(static_cast<Class*>(inClass)->*m_Member); }

private:
    T Class::*m_Member;
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

