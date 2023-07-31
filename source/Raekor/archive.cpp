#include "pch.h"
#include "archive.h"
#include "rtti.h"

namespace Raekor::JSON {

JSONArchive::JSONArchive(const Path& inPath) {
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
				m_Strings[index] = std::string(&m_StrBuffer[token.start], token.end - token.start);
				m_Primitives[index] = std::atof(m_Strings[index].c_str());
			}
			else if (c == 't' || c == 'f')
				m_Primitives[index] = 't' ? (double)true : (double)false;

		}
		else if (token.type == JSMN_STRING)
			m_Strings[index] = std::string(&m_StrBuffer[token.start], token.end - token.start);
	}
}

}
