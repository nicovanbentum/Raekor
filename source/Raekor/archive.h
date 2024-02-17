#pragma once

#include "serial.h"
#include "json.h"
#include "rtti.h"

namespace Raekor {

class BinaryReadArchive
{
public:
	BinaryReadArchive(const Path& inPath) : m_File(inPath, std::ios::binary | std::ios::in) {}

	template<typename T>
	BinaryReadArchive& operator>> (T& ioRHS)
	{
		std::string type;
		ReadFileBinary(m_File, type);

		if (auto rtti = g_RTTIFactory.GetRTTI(type.c_str()))
			for (const auto& member : *rtti)
			{
				if (member->GetSerializeType() & SERIALIZE_BINARY)
					member->FromBinary(m_File, &ioRHS);
			}


		return *this;
	}

	bool IsEOF() { return m_File.peek() != EOF; }

	File& GetFile() { return m_File; }

private:
	File m_File;
};


class BinaryWriteArchive
{
public:
	BinaryWriteArchive(const Path& inPath) : m_File(inPath, std::ios::binary | std::ios::out) {}

	template<typename T>
	BinaryWriteArchive& operator<< (const T& ioRHS)
	{
		WriteFileBinary(m_File, ioRHS);
		return *this;
	}

	template<typename T> requires HasRTTI<T>
	BinaryWriteArchive& operator<< (const T& ioRHS)
	{
		auto& rtti = gGetRTTI<T>();
		auto type = std::string(rtti.GetTypeName());

		WriteFileBinary(m_File, type);

		for (const auto& member : rtti)
		{
			if (member->GetSerializeType() & SERIALIZE_BINARY)
				member->ToBinary(m_File, &ioRHS);
		}

		return *this;
    }

    void WriteObject(const RTTI& inRTTI, void* inObject);

	File& GetFile() { return m_File; }

private:
	File m_File;
};

} // namespace raekor

namespace Raekor::JSON {

class ReadArchive
{
public:
	ReadArchive() = default;
    ReadArchive(const Path& inPath) : m_JSON(inPath) {}

	template<typename T>
	ReadArchive& operator>> (T& ioRHS);

     void* ReadNextObject(RTTI** rtti);

    bool IsEmpty() const { return m_JSON.IsEmpty(); }

private:
	JSONData m_JSON;
    int m_TokenIndex = 1;
};


class WriteArchive
{
public:
	WriteArchive() = default;
	WriteArchive(const Path& inPath) : m_JSON(inPath, true), m_Ofs(inPath)
	{
		m_Ofs << "{\n"; m_Writer.PushIndent();
	}
	~WriteArchive()
	{
		m_Ofs << "\n}";
	}

	template<typename T>
	WriteArchive& operator<< (T& inRHS);

    void WriteObject(const RTTI& inRTTI, void* inObject);

	JSONWriter& GetRaw() { return m_Writer; }

private:
	std::ofstream m_Ofs;
	std::vector<const char*> m_Types;

	JSONData m_JSON;
	JSONWriter m_Writer;
};



template<typename T>
ReadArchive& ReadArchive::operator>> (T& ioRHS)
{
    if (m_JSON.IsEmpty())
        return *this;

	// Any JSON document should start with a root object
	auto token_index = 0;
	if (m_JSON.GetToken(token_index).type != JSMN_OBJECT)
		return *this;

	token_index++;
	if (!m_JSON.IsKeyObjectPair(token_index)) // wrong format up ahead
		return *this;

	const auto& rtti = gGetRTTI<T>();

	// Keep skipping objects until we find a type match or hit EOF
	while (m_JSON.GetString(token_index) != rtti.GetTypeName())
	{
		token_index = m_JSON.SkipToken(token_index);

		if (token_index == -1) // hit end-of-file
			return *this;

		if (!m_JSON.IsKeyObjectPair(token_index)) // wrong format up ahead
			return *this;
	}

	// We only get here if we found a matching type
	assert(m_JSON.GetToken(token_index).type == JSMN_STRING);  // token index is on the type key
	token_index++; // increment index to type object

	// This nees to be calling the T& version of GetTokenToValue, 
	// which it should because we're calling gGetRTTI<T> so it has to be a registered complex type.. right?
	m_JSON.GetTokenToValue(token_index, ioRHS);

	return *this;
}


template<typename T>
WriteArchive& WriteArchive::operator<< (T& inRHS)
{
	const auto& rtti = gGetRTTI<T>();

	for (const auto& type_name : m_Types)
	{
		if (type_name == rtti.GetTypeName()) // ptr compare
			return *this;
	}

	if (m_Types.size() > 0)
		m_Ofs << ",\n";

	// write the type
	m_Writer.IndentAndWrite("\"").Write(rtti.GetTypeName()).Write("\":");

	// Convert and write the object to write to JSON
	m_Writer.GetValueToJSON(inRHS);

	m_Ofs << m_Writer.GetString();

	m_Writer.Clear();

	m_Types.push_back(rtti.GetTypeName());

	return *this;
}

} // Raekor::JSON

namespace Raekor { void RunArchiveTests(); }