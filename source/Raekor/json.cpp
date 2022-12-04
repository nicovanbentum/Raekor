#include "pch.h"
#include "json.h"

namespace Raekor::JSON {


char Parser::CurrChar() {
	return m_Source[m_Index];
}


bool Parser::NextChar(char& inChar) {
	if (m_Index + 1 >= m_Source.size())
		return false;

	while (std::isspace(char(m_Source[++m_Index])))
		if (m_Index + 1 >= m_Source.size())
			return false;

	inChar = m_Source[m_Index];
	return true;
}


bool Parser::PrevChar(char& inChar) {
	if (m_Index - 1 < 0)
		return false;

	auto index = m_Index;
	while (std::isspace(m_Source[--index]))
		if (m_Index - 1 < 0)
			return false;

	inChar = m_Source[index];
	return true;
}


bool Parser::PeekChar(char& inChar) {
	int index = m_Index;

	if (index + 1 >= m_Source.size())
		return false;

	while (std::isspace(m_Source[++index]))
		if (index + 1 >= m_Source.size())
			return false;

	inChar = m_Source[index];
	return true;
}


Value& Parser::GetCurrentValue() {
	switch (m_CurrentScope) {
		case Scope::OBJECT: 
			return m_Objects.back().at(m_Key);
		case Scope::ARRAY:  
			return m_Objects.back().at(m_Key).mArray.back();
	}
}


bool Parser::ParseString(std::string& inStr) {
	char c;
	auto begin_quote = m_Index;
	//move to the next char, which is either the first letter or closing quote
	if (!NextChar(c))
		return false;

	// consume until the cursor hits a quote
	while (c != '"') {
		if (!NextChar(c))
			return false;

		// if its an escape character, advance the cursor twice 
		if (c == '\\')
			for (int i = 0; i < 2; i++)
				if (!NextChar(c))
					return false;
	}

	inStr = m_Source.substr(begin_quote + 1, m_Index - begin_quote - 1);

	// consume the closing quote
	if (!NextChar(c))
		return false;

	return true;
}

bool Parser::ParseString() {
	char c;
	auto begin_quote = m_Index;
	//move to the next char, which is either the first letter or closing quote
	if (!NextChar(c))
		return false;

	// consume until the cursor hits a quote
	while (c != '"') {
		if (!NextChar(c))
			return false;

		// if its an escape character, advance the cursor twice 
		if (c == '\\')
			for (int i = 0; i < 2; i++)
				if (!NextChar(c))
					return false;
	}

	auto& token = GetCurrentValue();
	token.mType = ValueType::String;
	token.mData.mString.mPtr = &m_Source[begin_quote + 1];
	token.mData.mString.mLength = m_Index - begin_quote - 1;

	// consume the closing quote
	if (!NextChar(c))
		return false;

	return true;
}


bool Parser::ParseNumber() {
	char c;
	auto num_begin = m_Index;
	// move to the next char, which is either the second digit or something non-numerical
	if (!NextChar(c))
		return false;

	// TODO: properly handle the . - and e characters
	while (std::isdigit(c) || c == '.' || c == '-' || c == 'e')
		if (!NextChar(c))
			return false;


	auto& token = GetCurrentValue();
	token.mType = ValueType::Number;
	token.mData.mNumber = float(atof(m_Source.substr(num_begin, m_Index - num_begin).c_str()));

	return true;

}


bool Parser::ParseArray() {
	auto& token = m_Objects.back().at(m_Key);
	token.mType = ValueType::Array;

	m_CurrentScope = Scope::ARRAY;

	char c;
	while (CurrChar() != ']') {
		if (NextChar(c))
			token.mArray.emplace_back();
		else
			return false;

		if (!ParseValue())
			return false;

		if (CurrChar() != ',')
			break;
	}

	// consume the closing bracket
	if (!NextChar(c))
		return false;

	m_CurrentScope = Scope::OBJECT;
	return true;
}


bool Parser::ParseLiteral() {
	char c;
	auto literal_begin = m_Index;
	// move to the next char, which is either the second letter or something else
	if (!NextChar(c))
		return false;

	// consume all the following letters
	while (std::isalpha(c))
		if (!NextChar(c))
			return false;

	auto value = m_Source.substr(literal_begin, m_Index - literal_begin);
	auto& token = GetCurrentValue();

	if (value == "null") {
		token.mType = ValueType::Null;
	}
	else if (value == "true") {
		token.mType = ValueType::Bool;
		token.mData.mBool = true;
	}
	else if (value == "false") {
		token.mType = ValueType::Bool;
		token.mData.mBool = false;
	}
	else
		return false;

	return true;
}


bool Parser::ParseObject() {
	m_Objects.emplace_back();

	char c;
	while (CurrChar() != '}') {
	
		// opening curly brace should always be followed by the start of a string (key)
		if (!NextChar(c) || c != '"') {
			m_Objects.pop_back();
			return false;
		}

		// parse the key
		ParseString(m_Key);
		m_Objects.back().insert({m_Key, ValueType::Null});

		// make sure the key is delimited by a colon
		// everything after the colon is just another JSON value
		if (CurrChar() != ':' || !NextChar(c) || !ParseValue()) {
			m_Objects.pop_back();
			return false;
		}
	}

	// consume the closing curly brace
	// We don't care if NextChar failed or not here, because } is a valid EOF
	NextChar(c);

	return true;
}


bool Parser::ParseValue(Value* outValue) {
	char c = CurrChar();
	if (c == '{')
		return ParseObject();
	if (c == '[')
		return ParseArray();
	if (c == '"')
		return ParseString();
	if (std::isdigit(c) || (c == '-'))
		return ParseNumber();
	if (std::isalpha(c))
		return ParseLiteral();

	return false;
}


bool Parser::Parse() {
	if (m_Source.empty())
		return false;

	char c;
	m_Index = -1;

	while (NextChar(c)) {
		if (c == '{')
			if (!ParseObject())
				return false;
			else
				--m_Index;
	}

	return true;
}


ObjectBuilder::ObjectBuilder() {
	m_Stream << "{\n";
}

void ObjectBuilder::WriteValue(const Value& inValue) {
	switch (inValue.mType) {
	case ValueType::Null: {
		m_Stream << "\"null\"";
	} break;
	case ValueType::Bool: {
		if (inValue.mData.mBool) m_Stream << "true"; else m_Stream << "false";
	} break;
	case ValueType::Number: {
		m_Stream << inValue.mData.mNumber;
	} break;
	case ValueType::String: {
		m_Stream << '\"' << inValue.mData.mString.ToString() << '\"';
	} break;
	case ValueType::Array: {
		m_Stream << "[ ";

		for (int i = 0; i < inValue.mArray.size(); i++) {
			WriteValue(inValue.mArray[i]);
			
			if (i != inValue.mArray.size() - 1)
				m_Stream << ", ";
		}

		m_Stream << " ]";
	} break;
	default:
		assert(false);
	}
}


/* Writes a JSON key-value pair to the stream. */
ObjectBuilder& ObjectBuilder::WritePair(const std::string& inKey, const Value& inValue) {
	m_Stream << "\t\"" << inKey << "\": ";
	WriteValue(inValue);
	m_Stream << ",\n";
	return *this;
}


/* Writes a JSON key-value pair to the stream directly from strings. */
ObjectBuilder& ObjectBuilder::WritePair(const std::string& inKey, const std::string& inValue) {
	m_Stream << "\t\"" << inKey << "\": ";
	m_Stream << "\"" << inValue << "\",\n";
	return *this;
}


std::string ObjectBuilder::Build() {
	m_Stream.seekp(-2, m_Stream.cur);
	m_Stream << "\n}";
	return m_Stream.str();
}

} // raekor::json