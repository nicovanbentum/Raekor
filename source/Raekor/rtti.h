#pragma once

#include "hash.h"
#include "serial.h"

namespace Raekor::JSON 
{
	class JSONData;
	class JSONWriter;
}

template<typename T>
concept HasRTTI = requires ( T t ) { t.GetRTTI(); };

namespace Raekor {

class Member;

class RTTI
{
	friend class RTTIFactory;

	using CreateFn = void( * )( RTTI& rtti );
	using Constructor = void* ( * )( );

public:
	RTTI(const char* inName);
	RTTI(const char* inName, CreateFn inCreateFn, Constructor inConstructor);
	RTTI(RTTI&) = delete;
	RTTI(RTTI&&) = delete;

	void      AddMember(Member* inName);
	Member*   GetMember(uint32_t inIndex) const;
	Member*   GetMember(const char* inName) const; // O(n) Expensive!
	int32_t   GetMemberIndex(const char* inName) const; // O(n) expensive!
	uint32_t  GetMemberCount() const { return uint32_t(m_Members.size()); }

	void      AddBaseClass(RTTI& inRTTI);
	RTTI*     GetBaseClass(uint32_t inIndex) const;
	uint32_t  GetBaseClassCount() const { return uint32_t(m_BaseClasses.size()); }

	bool	  IsTypeOf(RTTI* InRTTI) const { return this == InRTTI; }
	bool      IsDerivedFrom(RTTI* inRTTI) const;

	bool operator==(const RTTI& rhs) const { return mHash == rhs.mHash; }
	bool operator!=(const RTTI& rhs) const { return mHash != rhs.mHash; }

	inline const char* GetTypeName() const { return m_Name.c_str(); }

	inline const auto end() const { return m_Members.end(); }
	inline const auto begin() const { return m_Members.begin(); }

	uint32_t mHash;

private:
	String m_Name;
	Constructor m_Constructor;
	Array<RTTI*> m_BaseClasses;
	Array<std::unique_ptr<Member>> m_Members;
};


class Member
{
public:
	Member() = delete;
	Member(const char* inName, const char* inCustomName, ESerializeType inSerializeType = SERIALIZE_JSON);
	virtual ~Member() = default;

	const char*		GetName() const { return m_Name; }
	uint32_t        GetNameHash() const { return m_NameHash; }
	const char*     GetCustomName() const { return m_CustomName; }
	ESerializeType  GetSerializeType() const { return m_SerializeType; }
	uint32_t        GetCustomNameHash() const { return m_CustomNameHash; }

	virtual void* GetPtr(void* inClass) = 0;
	virtual const void* GetPtr(const void* inClass) = 0;

	virtual RTTI* GetRTTI() { return nullptr; }

	template<typename T>
	T& GetRef(void* inClass) { return *static_cast<T*>( GetPtr(inClass) ); }
	template<typename T>
	const T& GetRef(const void* inClass) { return *static_cast<const T*>( GetPtr(inClass) ); }

	virtual void ToBinary(File& inFile, const void* inClass) {}
	virtual void FromBinary(File& inFile, void* inClass) {}

	virtual void     ToJSON(JSON::JSONWriter& inJSON, const void* inClass) {}
	virtual uint32_t FromJSON(JSON::JSONData& inJSON, uint32_t inTokenIdx, void* inClass) { return 0; }

protected:
	uint32_t m_NameHash;
	uint32_t m_CustomNameHash;
	const char* m_Name;
	const char* m_CustomName;
	ESerializeType m_SerializeType;
};


class RTTIFactory
{
public:
	void Register(RTTI& inRTTI);

	RTTI* GetRTTI(uint32_t inHash);
	RTTI* GetRTTI(const char* inType);

	void* Construct(const char* inType);

	inline auto begin() const { return m_RegisteredTypes.begin(); }
	inline auto end() const { return m_RegisteredTypes.end(); }

private:
	std::unordered_map<uint32_t, RTTI*> m_RegisteredTypes;
};

extern RTTIFactory g_RTTIFactory;

} // namespace Raekor


namespace std {
template <> struct hash<Raekor::RTTI>
{
	inline size_t operator()(const Raekor::RTTI& rtti) const
	{
		return rtti.mHash;
	}
};
}

/// ENUM MACROS ///

#define RTTI_DECLARE_ENUM(type)                                 \
RTTI& sGetRTTI(type* inType);                                   \
namespace type##Namespace { void sImplRTTI(RTTI& inRTTI); };    \


#define RTTI_DEFINE_ENUM(type)                                                                           \
    RTTI& sGetRTTI(type* inType) {                                                                       \
        static auto rtti = RTTI(#type, &type##Namespace::sImplRTTI, []() -> void* { return new type; }); \
        return rtti;                                                                                     \
    }                                                                                                    \
    void type##Namespace::sImplRTTI(RTTI& inRTTI)                                                        \


#define RTTI_DEFINE_ENUM_MEMBER(enum_type, serial_type, custom_type_name, member_type) \
    inRTTI.AddMember(new EnumMember(#member_type, #custom_type_name, serial_type))


#define RTTI_ENUM_STRING_CONVERSIONS(enum_type)                              \
    inline enum_type gFromString(const char* inString) {                     \
        auto& rtti = RTTI_OF(enum_type);                                     \
        const auto index = rtti.GetMemberIndex(inString);                    \
        return index != -1 ? (enum_type)index : (enum_type)0;                \
    }                                                                        \
                                                                             \
inline const char* gToString(enum_type inType) {                             \
    auto& rtti = RTTI_OF(enum_type);                                         \
    return rtti.GetMember((uint32_t)inType)->GetName();                      \
}                                                                            \

/// TYPE DECLARATION MACROS ///

#define __RTTI_DECLARE_TYPE(type, virtual_qualifier)                                                                    \
public:                                                                                                                 \
    virtual_qualifier RTTI& GetRTTI() const;                                                                            \
    friend RTTI& sGetRTTI(const type* inType);                                                                          \
    static void sImplRTTI(RTTI& inRTTI)  

#define RTTI_DECLARE_TYPE(type) __RTTI_DECLARE_TYPE(type,)
#define RTTI_DECLARE_VIRTUAL_TYPE(type) __RTTI_DECLARE_TYPE(type, virtual)

/// TYPE DEFINITION MACROS ///

#define __RTTI_DEFINE_TYPE(type, factory_type, inline_qualifier)                                                        \
    inline_qualifier RTTI& sGetRTTI(const type* inType) {                                                               \
        static auto rtti = RTTI(#type, &type::sImplRTTI, []() -> void* { return factory_type; });                       \
        return rtti;                                                                                                    \
    }                                                                                                                   \
                                                                                                                        \
    inline_qualifier RTTI& type::GetRTTI() const {                                                                      \
        return sGetRTTI(this);                                                                                          \
    }                                                                                                                   \
                                                                                                                        \
    inline_qualifier void type::sImplRTTI(RTTI& inRTTI)    

#define RTTI_DEFINE_TYPE(type)              __RTTI_DEFINE_TYPE(type, new type,)
#define RTTI_DEFINE_TYPE_INLINE(type)       __RTTI_DEFINE_TYPE(type, new type, inline)
#define RTTI_DEFINE_TYPE_NO_FACTORY(type)   __RTTI_DEFINE_TYPE(type, nullptr,) 


#define RTTI_DEFINE_TYPE_INHERITANCE(derived_class_type, base_class_type) \
    inRTTI.AddBaseClass(RTTI_OF(base_class_type))

#define RTTI_DEFINE_MEMBER(class_type, serial_type, custom_type_string, member_type) \
    inRTTI.AddMember(new ClassMember<class_type, decltype(class_type::member_type)>(#member_type, custom_type_string, &class_type::member_type, nullptr, serial_type))

#define RTTI_DEFINE_SCRIPT_MEMBER(class_type, serial_type, rtti_of, custom_type_string, member_type) \
    inRTTI.AddMember(new ClassMember<class_type, decltype(class_type::member_type)>(#member_type, custom_type_string, &class_type::member_type, rtti_of, serial_type))

#define RTTI_DECLARE_TYPE_PRIMITIVE(type) RTTI& sGetRTTI(const type* inType)

#define RTTI_DEFINE_TYPE_PRIMITIVE(type)	\
	RTTI& sGetRTTI(const type* inType) {	\
		static auto rtti = RTTI(#type);		\
			return rtti;					\
	}

/// RTTI GETTERS / HELPERS ///

#define RTTI_OF(name) sGetRTTI((name*)nullptr)

template<typename T>
Raekor::RTTI& gGetRTTI() { return sGetRTTI(( std::remove_cv_t<T>* )nullptr); }

template<typename T>
uint32_t gGetTypeHash() { return gGetRTTI<T>().mHash; }


namespace Raekor
{
	RTTI_DECLARE_TYPE_PRIMITIVE(int);
	RTTI_DECLARE_TYPE_PRIMITIVE(bool);
	RTTI_DECLARE_TYPE_PRIMITIVE(float);
	RTTI_DECLARE_TYPE_PRIMITIVE(uint32_t);

	void gRegisterPrimitiveTypes();
}