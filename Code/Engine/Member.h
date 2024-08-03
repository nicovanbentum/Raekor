#pragma once

#include "rtti.h"
#include "json.h"

namespace RK {

template<typename Class, typename T>
class ClassMember : public Member
{
public:
	ClassMember() = delete;
	ClassMember(const char* inName, const char* inCustomName, T Class::* inMember, RTTI* inRTTI = nullptr, ESerializeType inSerializeType = SERIALIZE_JSON)
		: Member(inName, inCustomName, inSerializeType), m_RTTI(inRTTI), m_Member(inMember)
	{
	}

	void ToJSON(JSON::JSONWriter& inJSON, const void* inClass) override
	{
		inJSON.GetValueToJSON(GetRef<T>(inClass));
	}

	uint32_t FromJSON(JSON::JSONData& inJSON, uint32_t inTokenIdx, void* inClass) override
	{
		return inJSON.GetTokenToValue(inTokenIdx, GetRef<T>(inClass));
	}

	void ToBinary(File& inFile, const void* inClass) override
	{
		WriteFileBinary(inFile, GetRef<T>(inClass));
	}

	void FromBinary(File& inFile, void* inClass) override
	{
		ReadFileBinary(inFile, GetRef<T>(inClass));
	}

	virtual RTTI* GetRTTI() override { return m_RTTI; }

	virtual void* GetPtr(void* inClass) override { return &( static_cast<Class*>( inClass )->*m_Member ); }
	virtual const void* GetPtr(const void* inClass) override { return &( static_cast<const Class*>( inClass )->*m_Member ); }


private:
	T Class::* m_Member;
	RTTI* m_RTTI = nullptr;
};


class EnumMember : public Member
{
public:
	EnumMember(const char* inName, const char* inCustomName, ESerializeType inSerializeType = SERIALIZE_JSON)
		: Member(inName, inCustomName, inSerializeType)
	{
	}

	virtual void* GetPtr(void* inClass) override { return inClass; }
	virtual const void* GetPtr(const void* inClass) override { return inClass; }
};

}