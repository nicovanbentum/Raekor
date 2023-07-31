#pragma once

#include "json.h"
#include "rtti.h"

namespace Raekor::JSON {

class JSONArchive {
public:
	JSONArchive() = default;
	JSONArchive(const Path& inPath);

	const std::string& GetStringBuffer() const { return m_StrBuffer; }

	template<typename T>
	JSONArchive& operator>> (T& ioRHS) {
		auto token_index = 0;
		if (m_Tokens[token_index].type != JSMN_OBJECT) // Any JSON document should start with a root object
			return *this;
		
		token_index++;
		if (!IsKeyObjectPair(token_index))
			return *this;

		const auto& rtti = gGetRTTI<T>();

		// Keep skipping objects until we find a type match or hit EOF
		while (m_Strings[token_index] != rtti.GetTypeName()) {
			token_index = SkipToken(m_Tokens, token_index); // advance to the the first token after the end of the current one

			if (token_index == -1) // hit end-of-file
				return *this;

			if (!IsKeyObjectPair(token_index)) // wrong format up ahead
				return *this;
		}

		// We only get here if we found a matching type
		assert(m_Tokens[token_index].type == JSMN_STRING);  // token index is on the key
		const auto& object_token = m_Tokens[++token_index]; // increment index to object

		for (auto key_index = 0; key_index < object_token.size; key_index++) {
			const auto& key = m_Strings[++token_index]; // increment index to key

			token_index++; // increment index to value
			if (const auto member = rtti.GetMember(key)) {
				const auto& value_token = m_Tokens[token_index];

				// Object and Array values can span multiple tokens
				if (value_token.type == JSMN_OBJECT || value_token.type == JSMN_ARRAY) {
					auto token_index_end = SkipToken(m_Tokens, token_index);
					if (token_index_end == -1) // hit EOF?
						token_index_end = m_Tokens.size();

					const auto value_tokens = Slice(&m_Tokens[token_index], token_index_end - token_index);

					member->FromJSON(m_StrBuffer, value_tokens, &ioRHS);

					token_index = SkipToken(m_Tokens, token_index) - 1;
				} 
				else {
					// Primitives, Strings and null are just 1 token
					member->FromJSON(m_StrBuffer, Slice(&m_Tokens[token_index]), &ioRHS);
				}
			} 
			else { // key is not a valid member, skip the value to get to the next key
				token_index = SkipToken(m_Tokens, token_index) - 1;
			}
		}

		return *this;
	}

private:
	/* Check if the current and next token conform to the following JSON: "key" : { } */
	inline bool IsKeyObjectPair(int inIdx) { return m_Tokens[inIdx].type == JSMN_STRING && m_Tokens[inIdx + 1].type == JSMN_OBJECT; }

private:
	std::string m_StrBuffer;
	std::vector<jsmntok_t> m_Tokens;
	std::vector<double> m_Primitives;
	std::vector<std::string> m_Strings;
};




class WriteArchive {
public:
	WriteArchive(const Path& inPath);

	template<typename T>
	WriteArchive& operator<< (T& rhs) {
		
		auto& rtti = RTTI_OF(T);

		for (int i = 0; i < rtti.GetMemberCount(); i++) {
			auto member = rtti.GetMember(i);
		}

		// m_Ofs << obj.AsString() << "\n\n";

		return *this;
	}

private:
	std::ofstream m_Ofs;
};



} // Raekor::JSON