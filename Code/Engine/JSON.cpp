#include "pch.h"
#include "json.h"
#include "iter.h"

namespace RK::JSON {

JSONData::JSONData(const Path& inPath, bool inTokenizeOnly)
{
	auto ifs = std::ifstream(inPath);
	std::stringstream buffer;
	buffer << ifs.rdbuf();
	m_StrBuffer = buffer.str();

	jsmn_parser parser;
	jsmn_init(&parser);

	const auto nr_of_tokens = jsmn_parse(&parser, m_StrBuffer.c_str(), m_StrBuffer.size(), NULL, 0);
	// jsmn returns a negative error code for malformed / truncated JSON
	if (nr_of_tokens <= 0)
	{
		if (nr_of_tokens < 0)
			std::cout << std::format("[JSON] Failed to parse {}, error code {}\n", inPath.string(), nr_of_tokens);

		return;
	}

	jsmn_init(&parser);
	m_Tokens.resize(nr_of_tokens);
	const auto parse_result = jsmn_parse(&parser, m_StrBuffer.c_str(), m_StrBuffer.size(), m_Tokens.data(), m_Tokens.size());

	if (parse_result != nr_of_tokens)
	{
		m_Tokens.clear();
		return;
	}

	if (inTokenizeOnly)
		return;

	m_Strings.resize(nr_of_tokens);
	m_Primitives.resize(nr_of_tokens);

	for (const auto& [index, token] : gEnumerate(m_Tokens))
	{
		auto c = m_StrBuffer[token.start];

		if (token.type == JSMN_PRIMITIVE)
		{
			if (std::isdigit(c) || c == '.' || c == '-' || c == 'e')
			{
				std::from_chars(&m_StrBuffer[m_Tokens[index].start], &m_StrBuffer[m_Tokens[index].end], m_Primitives[index]);
			}
			else if (c == 't' || c == 'f')
				m_Primitives[index] = c == 't' ? (double)true : (double)false;

		}
		else if (token.type == JSMN_STRING)
		{
			// undo the escaping done by JSONWriter::WriteValue(const std::string&)
			String& string = m_Strings[index];
			string.reserve(token.end - token.start);

			for (int char_index = token.start; char_index < token.end; char_index++)
			{
				const char current = m_StrBuffer[char_index];

				if (current == '\\' && char_index + 1 < token.end && ( m_StrBuffer[char_index + 1] == '\\' || m_StrBuffer[char_index + 1] == '"' ))
				{
					string += m_StrBuffer[char_index + 1];
					char_index++;
				}
				else
					string += current;
			}
		}
	}
}


bool JSONData::IsKeyObjectPair(uint32_t inIdx) const
{
	return m_Tokens[inIdx].type == JSMN_STRING && m_Tokens[inIdx + 1].type == JSMN_OBJECT;
}


uint32_t JSONData::SkipToken(uint32_t inTokenIdx) const
{
	if (!( inTokenIdx < m_Tokens.size() ))
		return inTokenIdx;

	const auto token_end = m_Tokens[inTokenIdx].end;

	while (m_Tokens[inTokenIdx].start < token_end)
	{
		inTokenIdx++;

		if (inTokenIdx >= m_Tokens.size())
			return -1;
	}

	return inTokenIdx;
}

} // raekor::JSON