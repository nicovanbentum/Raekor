#pragma once

#include "json.h"
#include "rtti.h"

namespace Raekor::JSON {

class ReadArchive {
public:
	ReadArchive() = default;
	ReadArchive(JSONData& inJSON) : m_JSON(inJSON) {}

	template<typename T>
	ReadArchive& operator>> (T& ioRHS);

private:
	JSONData& m_JSON;
};



class WriteArchive {
public:
	WriteArchive() = default;
	WriteArchive(const Path& inPath) : m_JSON(inPath), m_FilePath(inPath) {}

	template<typename T>
	WriteArchive& operator<< (T& inRHS) {
		auto ofs = std::ofstream(m_FilePath);

		m_Writer.Write("{\n").PushIndent();

		const auto& rtti = gGetRTTI<T>();

		// write the type
		m_Writer.IndentAndWrite("\"" + std::string(rtti.GetTypeName()) + "\":");

		// Convert and write the object to write to JSON
		m_Writer.GetValueToJSON(inRHS);

		m_Writer.Write("\n").PopIndent().Write("}");

		ofs << m_Writer.GetString();

		return *this;
	}



private:
	Path m_FilePath;
	JSONData& m_JSON;
	JSONWriter m_Writer;
};



template<typename T>
ReadArchive& ReadArchive::operator>> (T& ioRHS) {
	// Any JSON document should start with a root object
	auto token_index = 0;
	if (m_JSON.GetToken(token_index).type != JSMN_OBJECT)
		return *this;

	token_index++;
	if (!m_JSON.IsKeyObjectPair(token_index)) // wrong format up ahead
		return *this;

	const auto& rtti = gGetRTTI<T>();

	// Keep skipping objects until we find a type match or hit EOF
	while (m_JSON.GetString(token_index) !=  rtti.GetTypeName()) {
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

} // Raekor::JSON