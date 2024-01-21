#include "pch.h"
#include "archive.h"
#include "rtti.h"

namespace Raekor {

void* JSON::ReadArchive::ReadNextObject(RTTI** rtti)
{
    if (m_JSON.IsEmpty())
        return nullptr;

    // Any JSON document should start with a root object
    if (m_JSON.GetToken(0).type != JSMN_OBJECT)
        return nullptr;

    if (m_TokenIndex >= m_JSON.GetTokenCount() - 1)
        return nullptr;

    if (!m_JSON.IsKeyObjectPair(m_TokenIndex)) // wrong format up ahead
        return nullptr;

    // token index is on the type key
    assert(m_JSON.GetToken(m_TokenIndex).type == JSMN_STRING);  
    const std::string& type_name = m_JSON.GetString(m_TokenIndex);

    // look up the RTTI and allocate an instance from it
    *rtti = g_RTTIFactory.GetRTTI(type_name.c_str());
    void* object = g_RTTIFactory.Construct(type_name.c_str());

    // increment from type key to object, get the token
    const auto& object_token = m_JSON.GetToken(++m_TokenIndex);

    m_TokenIndex++; // increment index to first key (name of the first class member)

    for (auto key_index = 0; key_index < object_token.size; key_index++)
    {
        const auto& key_string = m_JSON.GetString(m_TokenIndex); // member name

        m_TokenIndex++; // increment index to value
        if (const auto member = (*rtti)->GetMember(key_string.c_str()))
        {
            // parse the current value, increment the token index by how many we have parsed
            m_TokenIndex = member->FromJSON(m_JSON, m_TokenIndex, object);
        }
        else
        { 
            // key is not a valid member, skip the value to get to the next key
            m_TokenIndex = m_JSON.SkipToken(m_TokenIndex);
        }
    }

    return object;
}


void BinaryWriteArchive::WriteObject(const RTTI& inRTTI, void* inObject)
{
    WriteFileBinary(m_File, std::string(inRTTI.GetTypeName()));

    for (const auto& member : inRTTI)
    {
        if (member->GetSerializeType() & SERIALIZE_BINARY)
        member->ToBinary(m_File, inObject);
    }
}

}

