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


 Member* RTTI::GetMember(uint32_t inIndex) { 
    return m_Members[inIndex].get(); 
}


void RTTIFactory::Register(RTTI& inRTTI) {
    assert(global->m_RegisteredTypes.find(inRTTI.GetHash()) == global->m_RegisteredTypes.end());
    global->m_RegisteredTypes[inRTTI.GetHash()] = &inRTTI;
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


void OutWriteData(OutArchive& ioArchive, const std::string& inData) {
    
}




} // namespace Raekor

