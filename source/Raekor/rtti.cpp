#include "pch.h"
#include "rtti.h"
#include "util.h"

namespace Raekor {

RTTI::RTTI(const char* inName, CreateFn inCreateFn, Constructor inConstructor)
    : m_Hash(gHash32Bit(inName)), m_Name(inName), m_Constructor(inConstructor) {
	    inCreateFn(*this);
}


void RTTI::AddMember(Member* inMember) {
	m_Members.emplace_back(std::unique_ptr<Member>(inMember));
}


 Member* RTTI::GetMember(uint32_t inIndex) const { 
    return m_Members[inIndex].get(); 
}


 Member* RTTI::GetMember(const char* inName) const {
     const auto name_hash = gHash32Bit(inName);
     for (const auto& member : m_Members)
         if (name_hash == member->GetNameHash() || name_hash == member->GetCustomNameHash())
             return member.get();

     return nullptr;
 }


int32_t RTTI::GetMemberIndex(const char* inName) const {
    const auto name_hash = gHash32Bit(inName);
     for (const auto& [index, member] : gEnumerate(m_Members))
         if (name_hash == member->GetNameHash() || name_hash == member->GetCustomNameHash())
             return index;

     return -1;
 }


 Member::Member(const char* inName, const char* inCustomName, ESerializeType inSerializeType)
     : m_NameHash(gHash32Bit(inName)), m_CustomNameHash(gHash32Bit(inCustomName)), m_Name(inName), m_CustomName(inCustomName), m_SerializeType(inSerializeType) {}

void RTTIFactory::Register(RTTI& inRTTI) {
    if (global->m_RegisteredTypes.find(inRTTI.GetHash()) == global->m_RegisteredTypes.end())
        global->m_RegisteredTypes[inRTTI.GetHash()] = &inRTTI;
}


void RTTI::AddBaseClass(RTTI& inRTTI) {
    m_BaseClasses.push_back(&inRTTI);
}


RTTI* RTTI::GetBaseClass(uint32_t inIndex) const {
    return m_BaseClasses[inIndex];
}


bool RTTI::IsDerivedFrom(RTTI* inRTTI) const {
    if (this == inRTTI)
        return true;

    for (auto base_class : m_BaseClasses)
        if (base_class->IsDerivedFrom(inRTTI))
            return true;

    return false;
}



RTTI* RTTIFactory::GetRTTI(uint32_t inHash) {
    auto rtti = global->m_RegisteredTypes.find(inHash);

    if (rtti != global->m_RegisteredTypes.end())
        return rtti->second;

    return nullptr;
}


RTTI* RTTIFactory::GetRTTI(const char* inType) {
    return GetRTTI(gHash32Bit(inType));
}

void* RTTIFactory::Construct(const char* inType) {
    auto rtti = GetRTTI(inType);
    if (!rtti)
        return nullptr;

    return rtti->m_Constructor();
}

} // namespace Raekor

