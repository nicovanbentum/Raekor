#include "pch.h"
#include "rtti.h"
#include "iter.h"
#include "hash.h"

namespace RK {

RTTIFactory	g_RTTIFactory;


RTTI::RTTI(const char* inName)
	: mHash(gHash32Bit(inName)), m_Name(inName)
{
}


RTTI::RTTI(const char* inName, CreateFn inCreateFn, Constructor inConstructor)
	: mHash(gHash32Bit(inName)), m_Name(inName), m_Constructor(inConstructor)
{
	inCreateFn(*this);
}


void RTTI::AddMember(Member* inMember)
{
	m_Members.emplace_back(std::unique_ptr<Member>(inMember));
}


Member* RTTI::GetMember(uint32_t inIndex) const
{
	return m_Members[inIndex].get();
}


Member* RTTI::GetMember(const char* inName) const
{
	const uint32_t name_hash = gHash32Bit(inName);
	for (const auto& member : m_Members)
		if (name_hash == member->GetNameHash() || name_hash == member->GetCustomNameHash())
			return member.get();

	return nullptr;
}


int32_t RTTI::GetMemberIndex(const char* inName) const
{
	const uint32_t name_hash = gHash32Bit(inName);
	for (const auto& [index, member] : gEnumerate(m_Members))
		if (name_hash == member->GetNameHash() || name_hash == member->GetCustomNameHash())
			return index;

	return -1;
}


Member::Member(const char* inName, const char* inCustomName, ESerializeType inSerializeType)
	: m_NameHash(gHash32Bit(inName)), m_CustomNameHash(gHash32Bit(inCustomName)), m_Name(inName), m_CustomName(inCustomName), m_SerializeType(inSerializeType)
{
}

void RTTIFactory::Register(RTTI& inRTTI)
{
	if (m_RegisteredTypes.find(inRTTI.mHash) == m_RegisteredTypes.end())
		m_RegisteredTypes[inRTTI.mHash] = &inRTTI;
}


void RTTI::AddBaseClass(RTTI& inRTTI)
{
	m_BaseClasses.push_back(&inRTTI);
}


RTTI* RTTI::GetBaseClass(uint32_t inIndex) const
{
	return m_BaseClasses[inIndex];
}


bool RTTI::IsDerivedFrom(RTTI* inRTTI) const
{
	if (this == inRTTI)
		return true;

	for (RTTI* base_class : m_BaseClasses)
		if (base_class->IsDerivedFrom(inRTTI))
			return true;

	return false;
}



RTTI* RTTIFactory::GetRTTI(uint32_t inHash)
{
	if (m_RegisteredTypes.contains(inHash))
		return m_RegisteredTypes.at(inHash);
	else
		return nullptr;
}


RTTI* RTTIFactory::GetRTTI(const char* inType)
{
	return GetRTTI(gHash32Bit(inType));
}

void* RTTIFactory::Construct(const char* inType)
{
	if (RTTI* rtti = GetRTTI(inType))
		return rtti->m_Constructor();
	else
		return nullptr;
}

} // namespace Raekor

RTTI_DEFINE_TYPE_PRIMITIVE(int);
RTTI_DEFINE_TYPE_PRIMITIVE(bool);
RTTI_DEFINE_TYPE_PRIMITIVE(float);
RTTI_DEFINE_TYPE_PRIMITIVE(uint32_t);

void gRegisterPrimitiveTypes()
{
	RK::g_RTTIFactory.Register<int>();
	RK::g_RTTIFactory.Register<bool>();
	RK::g_RTTIFactory.Register<float>();
	RK::g_RTTIFactory.Register<uint32_t>();
}
