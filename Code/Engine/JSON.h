#pragma once

#include "RTTI.h"
#include "Maths.h"
#include "Serialization.h"

namespace RK::JSON {

class JSONData
{
public:
	JSONData() = default;
	JSONData(const Path& inPath, bool inTokenizeOnly = false);

	bool IsKeyObjectPair(uint32_t inTokenIdx) const;
	uint32_t SkipToken(uint32_t inTokenIdx) const;

	uint32_t GetTokenCount() const { return m_Tokens.size(); }
	Array<jsmntok_t> GetTokens() const { return m_Tokens; }

	bool IsEmpty() const { return m_StrBuffer.empty() || m_Tokens.empty(); }
	bool HasRootObject() const { return m_Tokens.size() > 0 && m_Tokens[0].type == JSMN_OBJECT; }

	const jsmntok_t& GetToken(uint32_t inTokenIdx) const { return m_Tokens[inTokenIdx]; }
	const String& GetString(uint32_t inTokenIdx) const { return m_Strings[inTokenIdx]; }
	double GetPrimitive(uint32_t inTokenIdx) const { return m_Primitives[inTokenIdx]; }

public:
	// All return index to the next token
	// Generics
	template<typename T> requires HasRTTI<T>
	uint32_t ReadValue(uint32_t inTokenIdx, T& inValue);

	template<typename T> requires std::is_enum_v<T>
	uint32_t ReadValue(uint32_t inTokenIdx, T& inValue);

	template<typename T> requires std::is_arithmetic_v<T>
	uint32_t ReadValue(uint32_t inTokenIdx, T& inValue);

	// Primitives
	uint32_t ReadValue(uint32_t inTokenIdx, bool& inValue);
	uint32_t ReadValue(uint32_t inTokenIdx, std::string& inValue);

	// Math Types 
	template<glm::length_t L, typename T>
	uint32_t ReadValue(uint32_t inTokenIdx, glm::vec<L, T>& inValue);
	template<glm::length_t C, glm::length_t R, typename T>
	uint32_t ReadValue(uint32_t inTokenIdx, glm::mat<C, R, T>& inValue);
	uint32_t ReadValue(uint32_t inTokenIdx, glm::quat& inValue);

	// Containers
	template<typename T>
	uint32_t ReadValue(uint32_t inTokenIdx, std::vector<T>& inValue);
	template<typename T, uint32_t N>
	uint32_t ReadValue(uint32_t inTokenIdx, std::array<T, N>& inValue);
	template<typename T1, typename T2>
	uint32_t ReadValue(uint32_t inTokenIdx, std::pair<T1, T2>& inValue);
	template<typename K, typename V>
	uint32_t ReadValue(uint32_t inTokenIdx, std::unordered_map<K, V>& inValue);

	// Other
	uint32_t ReadValue(uint32_t inTokenIdx, Path& inValue);
	template<typename ...Types>
	uint32_t ReadValue(uint32_t inTokenIdx, std::variant<Types...>& inValue);

private:
	String m_StrBuffer;
	Array<jsmntok_t> m_Tokens;
	Array<double> m_Primitives;
	Array<String> m_Strings;
};



class JSONWriter
{
public:
	template<typename T>
	void IndentAndWrite(const T& inValue)
	{
		WriteIndent();
		m_SS << inValue;
	}

	template<typename T>
	void Write(const T& inValue)
	{
		m_SS << inValue;
	}

	void WriteIndent()
	{
		for (uint32_t i = 0; i < m_Indent; i++)
			m_SS << "    ";
	}

	void PopIndent()
	{ 
		m_Indent--; 
	}
	
	void PushIndent()
	{ 
		m_Indent++; 
	}

	String GetString() const 
	{ 
		return m_SS.str(); 
	}

	void Clear() 
	{ 
		m_SS = {}; 
	}

	void WriteToFile(const Path& inPath)
	{
		auto ofs = std::ofstream(inPath);
		ofs << GetString();
	}

public:
	// Generics
	template<typename T> requires HasRTTI<T>
	void WriteValue(const T& inValue);

	template<typename T> requires std::is_enum_v<T>
	void WriteValue(const T& inValue);

	template<typename T> requires std::is_arithmetic_v<T>
	void WriteValue(const T& inValue);

	// Primitives
	void WriteValue(const bool& inValue);
	void WriteValue(const std::string& inValue);

	// Math Types 
	template<glm::length_t L, typename T>
	void WriteValue(const glm::vec<L, T>& inValue);

	template<glm::length_t C, glm::length_t R, typename T>
	void WriteValue(const glm::mat<C, R, T>& inValue);

	void WriteValue(const glm::quat& inValue);

	// Containers
	template<typename T>
	void WriteValue(const std::vector<T>& inValue);

	template<typename T> requires std::is_integral_v<T> // integer arrays print 6 values on the same line
	void WriteValue(const std::vector<T>& inValue);

	template<typename T1, typename T2>
	void WriteValue(const std::pair<T1, T2>& inValue);

	template<typename T, uint32_t N>
	void WriteValue(const std::array<T, N>& inValue);

	template<typename K, typename V>
	void WriteValue(const std::unordered_map<K, V>& inValue);

	// Other
	void WriteValue(const Path& inValue);

private:
	int32_t m_Indent = 0;
	std::stringstream m_SS;
};


template<typename T> requires HasRTTI<T>
inline uint32_t JSONData::ReadValue(uint32_t inTokenIdx, T& inValue)
{
	const jsmntok_t& object_token = m_Tokens[inTokenIdx]; // index to object
	// T& can only deal with JSMN_OBJECT
	if (object_token.type != JSMN_OBJECT)
		return SkipToken(inTokenIdx);

	RTTI& rtti = RTTI_OF<T>();
	inTokenIdx++; // increment index to first key (name of the first class member)

	for (int key_index = 0; key_index < object_token.size; key_index++)
	{
		const String& key_string = GetString(inTokenIdx); // member name

		inTokenIdx++; // increment index to value
		if (Member* member = rtti.GetMember(key_string.c_str()))
		{
			// parse the current value, increment the token index by how many we have parsed
			inTokenIdx = member->FromJSON(*this, inTokenIdx, &inValue);
		}
		else
		{ // key is not a valid member, skip the value to get to the next key
			inTokenIdx = SkipToken(inTokenIdx);
		}
	}

	return inTokenIdx;
}

template<typename T> requires std::is_enum_v<T>
uint32_t JSONData::ReadValue(uint32_t inTokenIndex, T& inValue)
{
	inValue = (T)GetPrimitive(inTokenIndex++); 
	return inTokenIndex;
}

template<typename T> requires std::is_arithmetic_v<T>
inline uint32_t JSONData::ReadValue(uint32_t inTokenIndex, T& inValue)
{
	inValue = GetPrimitive(inTokenIndex++); 
	return inTokenIndex;
}

inline uint32_t JSONData::ReadValue(uint32_t inTokenIndex, bool& ioBool)
{
	ioBool = GetPrimitive(inTokenIndex++); 
	return inTokenIndex;
}

inline uint32_t JSONData::ReadValue(uint32_t inTokenIndex, std::string& ioString)
{
	ioString = GetString(inTokenIndex++); 
	return inTokenIndex;
}

template<glm::length_t L, typename T>
inline uint32_t JSONData::ReadValue(uint32_t inTokenIndex, glm::vec<L, T>& inVec)
{
	inVec = gFromString<L, T>(GetString(inTokenIndex++)); 
	return inTokenIndex;
}

inline uint32_t JSONData::ReadValue(uint32_t inTokenIndex, glm::quat& inQuat)
{
	inQuat = gFromString(GetString(inTokenIndex++)); 
	return inTokenIndex;
}

template<glm::length_t C, glm::length_t R, typename T>
inline uint32_t JSONData::ReadValue(uint32_t inTokenIndex, glm::mat<C, R, T>& inMatrix)
{
	inMatrix = gFromString<C, R, T>(GetString(inTokenIndex++)); 
	return inTokenIndex;
}


template<typename T>
inline uint32_t JSONData::ReadValue(uint32_t inTokenIndex, std::vector<T>& inVector)
{
	const jsmntok_t& object_token = m_Tokens[inTokenIndex]; // index to object
	// T& can only deal with JSMN_ARRAY
	if (object_token.type != JSMN_ARRAY)
		return SkipToken(inTokenIndex);

	// allocate storage
	inVector.resize(object_token.size);

	inTokenIndex++; // increment index to vector[0]
	for (int key_index = 0; key_index < object_token.size; key_index++)
		inTokenIndex = ReadValue(inTokenIndex, inVector[key_index]);

	return inTokenIndex;
}

template<typename T, uint32_t N>
inline uint32_t JSONData::ReadValue(uint32_t inTokenIndex, std::array<T, N>& inArray)
{
	const jsmntok_t& object_token = m_Tokens[inTokenIndex]; // index to object
	// T& can only deal with JSMN_ARRAY
	if (object_token.type != JSMN_ARRAY || object_token.size != N)
		return SkipToken(inTokenIndex);

	inTokenIndex++; // increment index to array[0]
	for (int key_index = 0; key_index < object_token.size; key_index++)
		inTokenIndex = ReadValue(inTokenIndex, inArray[key_index]);

	return inTokenIndex;
}

template<typename T1, typename T2>
uint32_t JSONData::ReadValue(uint32_t inTokenIndex, std::pair<T1, T2>& inPair)
{
	const jsmntok_t& object_token = m_Tokens[inTokenIndex]; // index to object
	// T& can only deal with JSMN_ARRAY
	if (object_token.type != JSMN_ARRAY || object_token.size != 2)
		return SkipToken(inTokenIndex);

	inTokenIndex++; // increment index to array[0]
	inTokenIndex = ReadValue(inTokenIndex, inPair.first);
	inTokenIndex = ReadValue(inTokenIndex, inPair.second);

	return inTokenIndex;
}


template<typename K, typename V>
uint32_t JSONData::ReadValue(uint32_t inTokenIndex, std::unordered_map<K, V>& inValue)
{
	const jsmntok_t& object_token = m_Tokens[inTokenIndex]; // index to object
	// T& can only deal with JSMN_OBJECT
	if (object_token.type != JSMN_OBJECT)
		return SkipToken(inTokenIndex);

	inTokenIndex++; // increment index to first key
	for (int key_index = 0; key_index < object_token.size; key_index++)
	{
		K key;
		inTokenIndex = ReadValue(inTokenIndex, key);

		V value;
		inTokenIndex = ReadValue(inTokenIndex, value);

		inValue[key] = value;
	}

	return inTokenIndex;
}


inline uint32_t JSONData::ReadValue(uint32_t inTokenIndex, Path& inPath)
{
	String value;
	const uint32_t result = ReadValue(inTokenIndex, value);
	inPath = Path(value);
	return result;
}

template<typename ...Types>
inline uint32_t JSONData::ReadValue(uint32_t inTokenIdx, std::variant<Types...>& inValue)
{
	// TODO
}


template<typename T> requires HasRTTI<T>
inline void JSONWriter::WriteValue(const T& inValue)
{
	Write("\n"); 
	IndentAndWrite("{\n"); 
	PushIndent();

	RTTI& rtti = RTTI_OF<T>();
	bool is_first_member = true;

	for (uint32_t i = 0; i < rtti.GetMemberCount(); i++)
	{
		// potentially skip
		if (( rtti.GetMember(i)->GetSerializeType() & SERIALIZE_JSON ) == 0)
			continue;
		// write delimiter, done before the key so skipped members can't leave a trailing comma
		if (!is_first_member)
			Write(",\n");
		is_first_member = false;
		// write key
		IndentAndWrite("\"");
		Write(rtti.GetMember(i)->GetCustomName());
		Write("\": ");
		// write value
		rtti.GetMember(i)->ToJSON(*this, &inValue);
	}

	Write("\n"); 
	PopIndent(); 
	IndentAndWrite("}");
}

template<typename T> requires std::is_enum_v<T>
inline void JSONWriter::WriteValue(const T& inValue)
{
	Write(std::to_string((int)inValue));
}

template<typename T> requires std::is_arithmetic_v<T>
inline void JSONWriter::WriteValue(const T& inValue)
{
	if constexpr (std::is_floating_point_v<T>)
	{
		// std::to_string uses %f which rounds to 6 decimals, to_chars writes the shortest string that round-trips exactly
		char buffer[64];
		const std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), inValue);
		Write(std::string_view(buffer, result.ptr));
	}
	else
	{
		Write(std::to_string(inValue));
	}
}

inline void JSONWriter::WriteValue(const bool& inBool)
{
	Write(inBool ? "true" : "false");
}

inline void JSONWriter::WriteValue(const std::string& inString)
{
	// escape quotes and backslashes so the string can't break the JSON structure, JSONData undoes this when parsing
	std::string escaped;
	escaped.reserve(inString.size() + 2);

	escaped += '"';

	for (char c : inString)
	{
		if (c == '"' || c == '\\')
			escaped += '\\';

		escaped += c;
	}

	escaped += '"';

	Write(escaped);
}

template<glm::length_t L, typename T>
inline void JSONWriter::WriteValue(const glm::vec<L, T>& inVec)
{
	Write("\"" + gToString(inVec) + "\"");

}

inline void JSONWriter::WriteValue(const glm::quat& inQuat)
{
	Write("\"" + gToString(inQuat) + "\"");

}

template<glm::length_t C, glm::length_t R, typename T>
inline void JSONWriter::WriteValue(const glm::mat<C, R, T>& inMatrix)
{
	Write("\"" + gToString(inMatrix) + "\"");
}


template<typename T>
inline void JSONWriter::WriteValue(const std::vector<T>& inVector)
{
	Write("\n"); 
	IndentAndWrite("[\n"); 
	PushIndent();

	const size_t count = inVector.size();

	for (uint64_t index = 0; index < count; index++)
	{
		WriteIndent();
		WriteValue(inVector[index]);

		if (index != count - 1)
			Write(",\n");
	}

	Write("\n"); 
	PopIndent(); 
	IndentAndWrite("]");
}

template<typename T> requires std::is_integral_v<T>
inline void JSONWriter::WriteValue(const std::vector<T>& inVector)
{
	Write("\n"); 
	IndentAndWrite("[\n");
	PushIndent();

	int count = inVector.size();
	int same_line = 9;

	for (int index = 0; index < count; index++)
	{
		if (same_line == 9)
			WriteIndent();

		WriteValue(inVector[index]);

		if (index != count - 1)
		{
			Write(", ");

			same_line--;
			if (same_line == 0)
			{
				Write("\n");
				same_line = 9;
			}
		}
	}

	Write("\n"); 
	PopIndent(); 
	IndentAndWrite("]");
}

template<typename T1, typename T2>
void JSONWriter::WriteValue(const std::pair<T1, T2>& inValue)
{
	Write("\n");
	IndentAndWrite("[");

	WriteValue(inValue.first);
	Write(", ");
	WriteValue(inValue.second);

	Write("]");
}

template<typename T, uint32_t N>
inline void JSONWriter::WriteValue(const std::array<T, N>& inArray)
{
	Write("\n"); 
	IndentAndWrite("[\n"); 
	PushIndent();

	for (int i = 0; i < N; i++)
	{
		WriteIndent();
		WriteValue(inArray[i]);

		if (i != N - 1)
			Write(",\n");
	}

	Write("\n");
	PopIndent();
	IndentAndWrite("]");
}

template<typename K, typename V>
void JSONWriter::WriteValue(const std::unordered_map<K, V>& inValue)
{
	Write("\n"); 
	IndentAndWrite("{\n"); 
	PushIndent();

	int index = 0;
	int count = inValue.size();

	for (const auto& [key, value] : inValue)
	{
		WriteIndent();
		WriteValue(key);
		Write(": ");
		WriteValue(value);

		if (index != count - 1)
			Write(",\n");

		index++;
	}

	Write("\n");
	PopIndent(); 
	IndentAndWrite("}");
}

inline void JSONWriter::WriteValue(const Path& inPath)
{
	Write("\"");
	Write(inPath.generic_string());
	Write("\"");
}

} // Raekor::JSON