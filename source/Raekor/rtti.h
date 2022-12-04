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
    Member*         GetMember(uint32_t inIndex);
    uint32_t        GetMemberCount() const { return m_Members.size(); }

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
    Member(const char* inName) : m_Name(inName) {}

    const char*     GetName() const { return m_Name; }
    virtual void*   GetMember(void* inClass) = 0;

    virtual void ToJSON(JSON::Value& ioValue, void* inClass) {}
    virtual void FromJSON(const JSON::Value& inValue, void* inClass) {}

    virtual bool IsPtr() { return false; }
    virtual void SetPtr() {}

    virtual void Load(ArchiveIn* ioArchive, void* inClass) {}
    virtual void Save(ArchiveOut* ioArchive, void* inClass) {}

private:
    const char* m_Name;
};


template<typename Class, typename T>
class ClassMember : public Member {
public:
    ClassMember() = delete;
    ClassMember(const char* inName, T Class::* inMember) : Member(inName), m_Member(inMember) {}
    
    virtual void Load(ArchiveIn* ioArchive, void* inClass) override {}
    virtual void Save(ArchiveOut* ioArchive, void* inClass) override {}

    virtual bool IsPtr() { return std::is_pointer_v<T>; }

    void ToJSON(JSON::Value& ioValue, void* inInstance) override { 
        ToJSONValue(ioValue, *static_cast<T*>(GetMember(inInstance)));
    }

    void FromJSON(const JSON::Value& inValue, void* inInstance) override {
        FromJSONValue(inValue, *static_cast<T*>(GetMember(inInstance)));
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

private:
    std::unordered_map<uint32_t, RTTI*> m_RegisteredTypes;

    // global singleton
    static inline std::unique_ptr<RTTIFactory> global = std::make_unique<RTTIFactory>();
};


class InArchive {

};

class OutArchive {

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


#define RTTI_CLASS_CPP_NO_FACTORY(name)                                                                                            \
    RTTI& sGetRTTI(name* inName) {                                                                                      \
        static auto rtti = RTTI(#name, &name::sImplRTTI, []() -> void* { return nullptr; });                           \
        return rtti;                                                                                                    \
    }                                                                                                                   \
                                                                                                                        \
    RTTI& name::GetRTTI() {                                                                                             \
        return sGetRTTI(this);                                                                                          \
    }                                                                                                                   \
                                                                                                                        \
    void name::sImplRTTI(RTTI& inRTTI)   


#define RTTI_MEMBER_CPP(class_name, custom_name, member_name)                                                           \
    inRTTI.AddMember(new ClassMember<class_name, decltype(class_name::member_name)>(#member_name, &class_name::member_name))
    

#define RTTI_OF(name) sGetRTTI((name*)nullptr)

template<typename T>
Raekor::RTTI& gGetRTTI() { return sGetRTTI((T*)nullptr); }