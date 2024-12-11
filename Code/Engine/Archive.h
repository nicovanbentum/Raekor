#pragma once

#include "RTTI.h"
#include "JSON.h"
#include "Serialization.h"

namespace RK {

class BinaryReadArchive
{
public:
	BinaryReadArchive(const Path& inPath) : m_File(inPath, std::ios::binary | std::ios::in) {}

	template<typename T>
	BinaryReadArchive& operator>> (T& ioRHS)
	{
		String type;
		ReadFileBinary(m_File, type);

		if (RTTI* rtti = g_RTTIFactory.GetRTTI(type.c_str()))
			for (const auto& member : *rtti)
			{
				if (member->GetSerializeType() & SERIALIZE_BINARY)
					member->FromBinary(m_File, &ioRHS);
			}


		return *this;
	}

	void ReadObject(void** inObject);

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
		RTTI& rtti = gGetRTTI<T>();
		String type = String(rtti.GetTypeName());

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

namespace RK::JSON {

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

	template<typename T> requires HasRTTI<T>
	WriteArchive& operator<< (T& inRHS);

    void WriteNextObject(const RTTI& inRTTI, void* inObject);

	JSONWriter& GetRaw() { return m_Writer; }

private:
	int m_Objects = 0;;
	std::ofstream m_Ofs;
	Array<const char*> m_Types;

	JSONData m_JSON;
	JSONWriter m_Writer;
};



template<typename T>
ReadArchive& ReadArchive::operator>> (T& ioRHS)
{
    if (m_JSON.IsEmpty())
        return *this;

	// Any JSON document should start with a root object
	if (m_JSON.GetToken(0).type != JSMN_OBJECT)
		return *this;

	if (m_TokenIndex >= m_JSON.GetTokenCount() - 1)
		return *this;

	if (!m_JSON.IsKeyObjectPair(m_TokenIndex)) // wrong format up ahead
		return *this;

	// token index is on the type key
	assert(m_JSON.GetToken(m_TokenIndex).type == JSMN_STRING);
	const String& type_name = m_JSON.GetString(m_TokenIndex);

	const RTTI& rtti = gGetRTTI<T>();

	// We only get here if we found a matching type
	assert(m_JSON.GetToken(m_TokenIndex).type == JSMN_STRING);  // token index is on the type key
	m_TokenIndex++; // increment index to type object

	// This nees to be calling the T& version of GetTokenToValue, 
	// which it should because we're calling gGetRTTI<T> so it has to be a registered complex type.. right?
	m_TokenIndex = m_JSON.GetTokenToValue(m_TokenIndex, ioRHS);

	return *this;
}


template<typename T> requires HasRTTI<T>
WriteArchive& WriteArchive::operator<< (T& inRHS)
{
	const RTTI& rtti = gGetRTTI<T>();

	for (const char* type_name : m_Types)
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

	m_Objects++;

	m_Writer.Clear();

	m_Types.push_back(rtti.GetTypeName());

	return *this;
}

} // Raekor::JSON

namespace RK { void RunArchiveTests(); }