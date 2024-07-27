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
	Slice<jsmntok_t> GetTokens() const { return Slice(m_Tokens); }

	bool IsEmpty() const { return m_StrBuffer.empty() || m_Tokens.empty(); }
	bool HasRootObject() const { return m_Tokens.size() > 0 && m_Tokens[0].type == JSMN_OBJECT; }

	const jsmntok_t& GetToken(uint32_t inTokenIdx) const { return m_Tokens[inTokenIdx]; }
	const std::string& GetString(uint32_t inTokenIdx) const { return m_Strings[inTokenIdx]; }
	double GetPrimitive(uint32_t inTokenIdx) const { return m_Primitives[inTokenIdx]; }

public:
	// Generics
	template<typename T> requires HasRTTI<T>
	uint32_t GetTokenToValue(uint32_t inTokenIdx, T& inValue);

	template<typename T> requires std::is_enum_v<T>
	uint32_t GetTokenToValue(uint32_t inTokenIdx, T& inValue);

	template<typename T> requires std::is_arithmetic_v<T>
	uint32_t GetTokenToValue(uint32_t inTokenIdx, T& inValue);

	// Primitives
	uint32_t GetTokenToValue(uint32_t inTokenIdx, bool& inValue);
	uint32_t GetTokenToValue(uint32_t inTokenIdx, std::string& inValue);

	// Math Types 
	template<glm::length_t L, typename T>
	uint32_t GetTokenToValue(uint32_t inTokenIdx, glm::vec<L, T>& inValue);
	template<glm::length_t C, glm::length_t R, typename T>
	uint32_t GetTokenToValue(uint32_t inTokenIdx, glm::mat<C, R, T>& inValue);
	uint32_t GetTokenToValue(uint32_t inTokenIdx, glm::quat& inValue);

	// Containers
	template<typename T>
	uint32_t GetTokenToValue(uint32_t inTokenIdx, std::vector<T>& inValue);
	template<typename T, uint32_t N>
	uint32_t GetTokenToValue(uint32_t inTokenIdx, std::array<T, N>& inValue);
	template<typename T1, typename T2>
	uint32_t GetTokenToValue(uint32_t inTokenIdx, std::pair<T1, T2>& inValue);
	template<typename K, typename V>
	uint32_t GetTokenToValue(uint32_t inTokenIdx, std::unordered_map<K, V>& inValue);

	// Other

	uint32_t GetTokenToValue(uint32_t inTokenIdx, Path& inValue);
	template<typename ...Types>
	uint32_t GetTokenToValue(uint32_t inTokenIdx, std::variant<Types...>& inValue);

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
	JSONWriter& IndentAndWrite(const T& inValue)
	{
		WriteIndent();
		m_SS << inValue;
		return *this;
	}

	template<typename T>
	JSONWriter& Write(const T& inValue)
	{
		m_SS << inValue;
		return *this;
	}

	JSONWriter& WriteIndent()
	{
		for (uint32_t i = 0; i < m_Indent; i++)
			m_SS << "    ";
		return *this;
	}

	JSONWriter& PopIndent() { m_Indent--; return *this; }
	JSONWriter& PushIndent() { m_Indent++; return *this; }

	std::string GetString() const { return m_SS.str(); }

	void Clear() { m_SS = {}; }

	void WriteToFile(const Path& inPath)
	{
		auto ofs = std::ofstream(inPath);
		ofs << GetString();
	}

public:
	// Generics
	template<typename T> requires HasRTTI<T>
	void GetValueToJSON(const T& inValue);

	template<typename T> requires std::is_enum_v<T>
	void GetValueToJSON(const T& inValue);

	template<typename T> requires std::is_arithmetic_v<T>
	void GetValueToJSON(const T& inValue);

	// Primitives
	void GetValueToJSON(const bool& inValue);
	void GetValueToJSON(const std::string& inValue);

	// Math Types 
	template<glm::length_t L, typename T>
	void GetValueToJSON(const glm::vec<L, T>& inValue);

	template<glm::length_t C, glm::length_t R, typename T>
	void GetValueToJSON(const glm::mat<C, R, T>& inValue);

	void GetValueToJSON(const glm::quat& inValue);

	// Containers
	template<typename T>
	void GetValueToJSON(const std::vector<T>& inValue);

	template<typename T> requires std::is_integral_v<T> // integer arrays print 6 values on the same line
	void GetValueToJSON(const std::vector<T>& inValue);

	template<typename T1, typename T2>
	void GetValueToJSON(const std::pair<T1, T2>& inValue);

	template<typename T, uint32_t N>
	void GetValueToJSON(const std::array<T, N>& inValue);

	template<typename K, typename V>
	void GetValueToJSON(const std::unordered_map<K, V>& inValue);

	// Other
	void GetValueToJSON(const Path& inValue);

private:
	int32_t m_Indent = 0;
	std::stringstream m_SS;
};


template<typename T> requires HasRTTI<T>
inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIdx, T& inValue)
{
	const jsmntok_t& object_token = m_Tokens[inTokenIdx]; // index to object
	// T& can only deal with JSMN_OBJECT
	if (object_token.type != JSMN_OBJECT)
		return SkipToken(inTokenIdx);

	RTTI& rtti = gGetRTTI<T>();
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
uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, T& inValue)
{
	inValue = (T)GetPrimitive(inTokenIndex++); return inTokenIndex;
}

template<typename T> requires std::is_arithmetic_v<T>
inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, T& inValue)
{
	inValue = GetPrimitive(inTokenIndex++); return inTokenIndex;
}

inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, bool& ioBool)
{
	ioBool = GetPrimitive(inTokenIndex++); return inTokenIndex;
}

inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, std::string& ioString)
{
	ioString = GetString(inTokenIndex++); return inTokenIndex;
}

template<glm::length_t L, typename T>
inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, glm::vec<L, T>& inVec)
{
	inVec = gFromString<L, T>(GetString(inTokenIndex++)); return inTokenIndex;
}

inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, glm::quat& inQuat)
{
	inQuat = gFromString(GetString(inTokenIndex++)); return inTokenIndex;
}

template<glm::length_t C, glm::length_t R, typename T>
inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, glm::mat<C, R, T>& inMatrix)
{
	inMatrix = gFromString<C, R, T>(GetString(inTokenIndex++)); return inTokenIndex;
}


template<typename T>
inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, std::vector<T>& inVector)
{
	const jsmntok_t& object_token = m_Tokens[inTokenIndex]; // index to object
	// T& can only deal with JSMN_ARRAY
	if (object_token.type != JSMN_ARRAY)
		return SkipToken(inTokenIndex);

	// allocate storage
	inVector.resize(object_token.size);

	inTokenIndex++; // increment index to vector[0]
	for (int key_index = 0; key_index < object_token.size; key_index++)
		inTokenIndex = GetTokenToValue(inTokenIndex, inVector[key_index]);

	return inTokenIndex;
}

template<typename T, uint32_t N>
inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, std::array<T, N>& inArray)
{
	const jsmntok_t& object_token = m_Tokens[inTokenIndex]; // index to object
	// T& can only deal with JSMN_ARRAY
	if (object_token.type != JSMN_ARRAY || object_token.size != N)
		return SkipToken(inTokenIndex);

	inTokenIndex++; // increment index to array[0]
	for (int key_index = 0; key_index < object_token.size; key_index++)
		inTokenIndex = GetTokenToValue(inTokenIndex, inArray[key_index]);

	return inTokenIndex;
}

template<typename T1, typename T2>
uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, std::pair<T1, T2>& inPair)
{
	const jsmntok_t& object_token = m_Tokens[inTokenIndex]; // index to object
	// T& can only deal with JSMN_ARRAY
	if (object_token.type != JSMN_ARRAY || object_token.size != 2)
		return SkipToken(inTokenIndex);

	inTokenIndex++; // increment index to array[0]
	inTokenIndex = GetTokenToValue(inTokenIndex, inPair.first);
	inTokenIndex = GetTokenToValue(inTokenIndex, inPair.second);

	return inTokenIndex;
}


template<typename K, typename V>
uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, std::unordered_map<K, V>& inValue)
{
	const jsmntok_t& object_token = m_Tokens[inTokenIndex]; // index to object
	// T& can only deal with JSMN_OBJECT
	if (object_token.type != JSMN_OBJECT)
		return SkipToken(inTokenIndex);

	inTokenIndex++; // increment index to first key
	for (int key_index = 0; key_index < object_token.size; key_index++)
	{
		K key;
		inTokenIndex = GetTokenToValue(inTokenIndex, key);

		V value;
		inTokenIndex = GetTokenToValue(inTokenIndex, value);

		inValue[key] = value;
	}

	return inTokenIndex;
}


inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, Path& inPath)
{
	String value;
	const uint32_t result = GetTokenToValue(inTokenIndex, value);
	inPath = Path(value);
	return result;
}

template<typename ...Types>
inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIdx, std::variant<Types...>& inValue)
{
	// TODO
}


template<typename T> requires HasRTTI<T>
inline void JSONWriter::GetValueToJSON(const T& inMember)
{
	Write("\n").IndentAndWrite("{\n").PushIndent();

	RTTI& rtti = gGetRTTI<T>();
	for (uint32_t i = 0; i < rtti.GetMemberCount(); i++)
	{
		// potentially skip
		if (( rtti.GetMember(i)->GetSerializeType() & SERIALIZE_JSON ) == 0)
			continue;
		// write key
		IndentAndWrite("\"").Write(rtti.GetMember(i)->GetCustomName()).Write("\": ");
		// write value
		rtti.GetMember(i)->ToJSON(*this, &inMember);
		// write delimiter
		if (i != rtti.GetMemberCount() - 1)
			Write(",\n");
	}

	Write("\n").PopIndent().IndentAndWrite("}");
}

template<typename T> requires std::is_enum_v<T>
inline void JSONWriter::GetValueToJSON(const T& inMember)
{
	Write(std::to_string((int)inMember));
}

template<typename T> requires std::is_arithmetic_v<T>
inline void JSONWriter::GetValueToJSON(const T& inValue)
{
	Write(std::to_string(inValue));
}

inline void JSONWriter::GetValueToJSON(const bool& inBool)
{
	Write(inBool ? "true" : "false");
}

inline void JSONWriter::GetValueToJSON(const std::string& inString)
{
	Write("\"");
	Write(inString);
	Write("\"");
}

template<glm::length_t L, typename T>
inline void JSONWriter::GetValueToJSON(const glm::vec<L, T>& inVec)
{
	Write("\"").Write(gToString(inVec)).Write("\"");
}

inline void JSONWriter::GetValueToJSON(const glm::quat& inQuat)
{
	Write("\"" + gToString(inQuat) + "\"");

}

template<glm::length_t C, glm::length_t R, typename T>
inline void JSONWriter::GetValueToJSON(const glm::mat<C, R, T>& inMatrix)
{
	Write("\"" + gToString(inMatrix) + "\"");
}


template<typename T>
inline void JSONWriter::GetValueToJSON(const std::vector<T>& inVector)
{
	Write("\n").IndentAndWrite("[\n").PushIndent();

	const size_t count = inVector.size();

	for (uint64_t index = 0; index < count; index++)
	{
		WriteIndent();
		GetValueToJSON(inVector[index]);

		if (index != count - 1)
			Write(",\n");
	}

	Write("\n").PopIndent().IndentAndWrite("]");
}

template<typename T> requires std::is_integral_v<T>
inline void JSONWriter::GetValueToJSON(const std::vector<T>& inVector)
{
	Write("\n").IndentAndWrite("[\n").PushIndent();

	int count = inVector.size();
	int same_line = 9;

	for (int index = 0; index < count; index++)
	{
		if (same_line == 9)
			WriteIndent();

		GetValueToJSON(inVector[index]);

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

	Write("\n").PopIndent().IndentAndWrite("]");
}

template<typename T1, typename T2>
void JSONWriter::GetValueToJSON(const std::pair<T1, T2>& inValue)
{
	Write("\n").IndentAndWrite("[");

	GetValueToJSON(inValue.first);
	Write(", ");
	GetValueToJSON(inValue.second);

	Write("]");
}

template<typename T, uint32_t N>
inline void JSONWriter::GetValueToJSON(const std::array<T, N>& inArray)
{
	Write("\n").IndentAndWrite("[\n").PushIndent();

	for (int i = 0; i < N; i++)
	{
		WriteIndent();
		GetValueToJSON(inArray[i]);

		if (i != N - 1)
			Write(",\n");
	}

	Write("\n").PopIndent().IndentAndWrite("]");
}

template<typename K, typename V>
void JSONWriter::GetValueToJSON(const std::unordered_map<K, V>& inValue)
{
	Write("\n").IndentAndWrite("{\n").PushIndent();

	int index = 0;
	int count = inValue.size();

	for (const auto& [key, value] : inValue)
	{
		WriteIndent();
		GetValueToJSON(key);
		Write(": ");
		GetValueToJSON(value);

		if (index != count - 1)
			Write(",\n");

		index++;
	}

	Write("\n").PopIndent().IndentAndWrite("}");
}

inline void JSONWriter::GetValueToJSON(const Path& inPath)
{
	Write("\"");
	Write(inPath.generic_string());
	Write("\"");
}

} // Raekor::JSON