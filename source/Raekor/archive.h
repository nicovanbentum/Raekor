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
	WriteArchive();
	WriteArchive(const Path& inPath) : m_Ofs(inPath) {}

	template<typename T>
	WriteArchive& operator<< (T& inRHS) {
		RTTI& rtti = gGetRTTI<T>();
		m_SS << "\"" << rtti.GetTypeName() << "\":";
		m_SS << "{";
		m_Indent++;

		for (uint32_t i = 0; i < rtti.GetMemberCount(); i++) {
			std::string json;
			rtti.GetMember(i)->ToJSON(json, &inRHS);
			m_SS << json;

			if (i < rtti.GetMemberCount() - 1)
				m_SS << ",";
		}

		m_Indent--;
		while (m_Indent >= 0) {
			m_SS << "}";
			m_Indent--;
		}

		const auto json_str = m_SS.str();


		m_Ofs << "{\n";
		
		auto indent = 1;
		for (auto i = 0; i < indent; i++)
			m_Ofs << "    ";

		for (uint64_t index = 0; index < json_str.size(); index++) {
			auto c = json_str[index];
			if (c == '{') {
				m_Ofs << "\n";
				for (auto i = 0; i < indent; i++)
					m_Ofs << "    ";
				m_Ofs << c << "\n";
				indent++;
				for (auto i = 0; i < indent; i++)
					m_Ofs << "    ";
			}
			else if (c == '}') {
				m_Ofs << "\n";
				indent--;
				for (auto i = 0; i < indent; i++)
					m_Ofs << "    ";
				m_Ofs << c;

				if (index != json_str.size() - 1) {
					auto next_c = json_str[index];

					if (next_c != ',' && next_c != '}')
						m_Ofs << "\n";
				}
			}
			else if (c == ',') {
				m_Ofs << c << "\n";
				for (auto i = 0; i < indent; i++)
					m_Ofs << "    ";
			}
			else {
				m_Ofs << c;
			}
		}

		m_Ofs << "\n}";

		return *this;
	}



private:
	int32_t m_Indent = 0;
	Path m_FilePath;
	std::ofstream m_Ofs;
	std::stringstream m_SS;
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