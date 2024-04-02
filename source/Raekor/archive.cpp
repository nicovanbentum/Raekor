#include "pch.h"
#include "archive.h"

#include "OS.h"
#include "rtti.h"
#include "member.h"

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

void JSON::WriteArchive::WriteNextObject(const RTTI& inRTTI, void* inObject)
{
    if (m_Objects > 0)
        m_Writer.Write(",\n");

    // write the type
    m_Writer.IndentAndWrite("\"").Write(inRTTI.GetTypeName()).Write("\":");

    m_Writer.Write("\n").IndentAndWrite("{\n").PushIndent();

    for (uint32_t i = 0; i < inRTTI.GetMemberCount(); i++)
    {
        // potentially skip
        if ((inRTTI.GetMember(i)->GetSerializeType() & SERIALIZE_JSON) == 0)
            continue;
        // write key
        m_Writer.IndentAndWrite("\"").Write(inRTTI.GetMember(i)->GetCustomName()).Write("\": ");
        // write value
        inRTTI.GetMember(i)->ToJSON(m_Writer, inObject);
        // write delimiter
        if (i != inRTTI.GetMemberCount() - 1)
            m_Writer.Write(",\n");
    }
    
    m_Writer.Write("\n").PopIndent().IndentAndWrite("}");

    m_Ofs << m_Writer.GetString();

    m_Objects++;

    m_Writer.Clear();
}


void BinaryReadArchive::ReadObject(void** inObject)
{
    std::string type_name;
    ReadFileBinary(m_File, type_name);

    *inObject = g_RTTIFactory.Construct(type_name.c_str());

    if (RTTI* rtti = g_RTTIFactory.GetRTTI(type_name.c_str()))
    {
        for (const auto& member : *rtti)
        {
            if (member->GetSerializeType() & SERIALIZE_BINARY)
                member->FromBinary(m_File, inObject);
        }
    }

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


struct TestStrings
{
    RTTI_DECLARE_TYPE(TestStrings);

    std::vector<std::string> Strings0;
    std::vector<std::string> Strings1;
};

struct Test
{
    RTTI_DECLARE_TYPE(Test);

    int Integer = 0;
    std::string String;
    std::unordered_map<std::string, std::string> Map;
    std::unordered_map<uint32_t, TestStrings> VectorMap;
};

RTTI_DEFINE_TYPE(TestStrings)
{
    RTTI_DEFINE_MEMBER(TestStrings, SERIALIZE_ALL, "Strings 0", Strings0);
    RTTI_DEFINE_MEMBER(TestStrings, SERIALIZE_ALL, "Strings 1", Strings1);
}

RTTI_DEFINE_TYPE(Test)
{
    RTTI_DEFINE_MEMBER(Test, SERIALIZE_ALL, "Integer", Integer);
    RTTI_DEFINE_MEMBER(Test, SERIALIZE_ALL, "String", String);
    RTTI_DEFINE_MEMBER(Test, SERIALIZE_ALL, "Map", Map);
    RTTI_DEFINE_MEMBER(Test, SERIALIZE_ALL, "VectorMap", VectorMap);
}

void RunArchiveTests()
{
    g_RTTIFactory.Register(RTTI_OF(Test));
    g_RTTIFactory.Register(RTTI_OF(TestStrings));

    const auto TEMP_FILE = OS::sGetTempPath() / "test.bin";

    {
        BinaryWriteArchive archive(TEMP_FILE);

        Test test;
        test.Integer = -1;
        test.String = "Hello World";
        test.Map["key0"] = "value0";
        test.Map["key1"] = "value1";
        test.Map["key2"] = "value2";

        test.VectorMap[5].Strings0.push_back("str0");
        test.VectorMap[5].Strings0.push_back("str1");
        test.VectorMap[12].Strings1.push_back("str2");
        test.VectorMap[12].Strings1.push_back("str3");

        archive << test;
    }

    {
        BinaryReadArchive archive(TEMP_FILE);
        
        Test test;
        archive >> test;

        assert(test.Integer == -1);
        assert(test.String == "Hello World");
        assert(test.Map["key0"] == "value0");
        assert(test.Map["key1"] == "value1");
        assert(test.Map["key2"] == "value2");

        assert(test.VectorMap[5].Strings0[0] == "str0");
        assert(test.VectorMap[5].Strings0[1] == "str1");
        assert(test.VectorMap[12].Strings1[0] == "str2");
        assert(test.VectorMap[12].Strings1[1] == "str3");
    }
}

}

