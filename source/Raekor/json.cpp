#include "pch.h"
#include "json.h"

namespace Raekor::JSON {

JSONData::JSONData(const Path& inPath) {
	auto ifs = std::ifstream(inPath);
	std::stringstream buffer;
	buffer << ifs.rdbuf();
	m_StrBuffer = buffer.str();

	jsmn_parser parser;
	jsmn_init(&parser);

	const auto nr_of_tokens = jsmn_parse(&parser, m_StrBuffer.c_str(), m_StrBuffer.size(), NULL, 0);
	if (nr_of_tokens == 0)
		return;

	jsmn_init(&parser);
	m_Tokens.resize(nr_of_tokens);
	m_Strings.resize(nr_of_tokens);
	m_Primitives.resize(nr_of_tokens);
	const auto parse_result = jsmn_parse(&parser, m_StrBuffer.c_str(), m_StrBuffer.size(), m_Tokens.data(), m_Tokens.size());

	if (parse_result != nr_of_tokens)
		return;

	for (const auto& [index, token] : gEnumerate(m_Tokens)) {
		auto c = m_StrBuffer[token.start];

		if (token.type == JSMN_PRIMITIVE) {
			if (std::isdigit(c) || c == '.' || c == '-' || c == 'e') {
				std::from_chars(&m_StrBuffer[m_Tokens[index].start], &m_StrBuffer[m_Tokens[index].end], m_Primitives[index]);
			}
			else if (c == 't' || c == 'f')
				m_Primitives[index] = 't' ? (double)true : (double)false;

		}
		else if (token.type == JSMN_STRING)
			m_Strings[index] = std::string(&m_StrBuffer[token.start], token.end - token.start);
	}
}


bool JSONData::IsKeyObjectPair(uint32_t inIdx) {
	return m_Tokens[inIdx].type == JSMN_STRING && m_Tokens[inIdx + 1].type == JSMN_OBJECT;
}


uint32_t JSONData::SkipToken(uint32_t inTokenIdx) {
	if (!(inTokenIdx < m_Tokens.size()))
		return inTokenIdx;

	const auto token_end = m_Tokens[inTokenIdx].end;

	while (m_Tokens[inTokenIdx].start < token_end) {
		inTokenIdx++;

		if (inTokenIdx >= m_Tokens.size())
			return -1;
	}

	return inTokenIdx;
}

} // raekor::JSON