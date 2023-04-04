#pragma once

#include "util.h"
#include "archive.h"

namespace Raekor::JSON {

enum class ValueType {
	Null,
	Bool,
	Number,
	String,
	Array
};

struct String {
    const char* mPtr;
    int32_t mLength;

    std::string ToString() const { return std::string(mPtr, mLength); }
};

struct Value {
	using Array = std::vector<Value>;

	Value() = default;
	Value(ValueType inType) : mType(inType) {}

	template<typename T> const T& As() const;
	template<> const bool&	 As() const { assert(mType == ValueType::Bool);   return mData.mBool;   }
	template<> const float&	 As() const { assert(mType == ValueType::Number); return mData.mNumber; }
	template<> const Array&	 As() const { assert(mType == ValueType::Array);  return mArray;		}
	template<> const String& As() const { assert(mType == ValueType::String); return mData.mString; }

	union Data {
		bool	mBool;
		float	mNumber;
		String	mString;
	} mData;

	Array mArray;
	
	ValueType mType = ValueType::Null;
};


using Object = std::unordered_map<std::string, Value>;


enum class ParseResult {
	OK,
	UNKOWN_VALUE,
	PARSE_OBJECT_FAIL,
	PARSE_STRING_FAIL,
	PARSE_NUMBER_FAIL,
	PARSE_ARRAY_FAIL,
	PARSE_LITERAL_FAIL,
};

enum class Scope {
	OBJECT,
	ARRAY
};

// TODO: Improve error reporting and runtime failure, GetValue shouldnt crash
class Parser {
public:
	Parser() = default;
	Parser(const std::string& inSrc) : m_Source(inSrc) {}

	bool			Parse();
	const Value&	GetValue(const Object& inObject, const std::string& inKey) const { return inObject.at(inKey); }
	bool			Contains(const Object& inObject, const std::string& inKey) const { return inObject.find(inKey) != inObject.end(); }
	bool			IsEmpty (const Object& inObject, const std::string& inKey) const { return Contains(inObject, inKey) && true; }

	const auto end() const { return m_Objects.end(); }
	const auto begin() const { return m_Objects.begin(); }

private:
	/* Returns the character at current cursor position. */
	char CurrChar();

	/* Writes the next non-whitespace character to outChar. Advances the cursor by consuming whitespace. */
	bool NextChar(char& outChar);

	/* Returns the character at previous cursor position. Does not alter the current cursor position. */
	bool PrevChar(char& outChar);

	/* Writes the next non-whitespace character to outChar. Does not advance the cursor. */
	bool PeekChar(char& outChar);

	/* Starts from a { character and consumes up to and including the } character. */
	bool ParseObject();

	/* Starts from a " character and consumes up to and including another " character. */
	bool ParseString();

	/* Starts from a " character and consumes up to and including another " character. Outputs the string to inStr, used for keys. */
	bool ParseString(std::string& inStr);

	/* Starts from a numerical character and consumes up to and including the last character of a sequence of numerical characters. */
	bool ParseNumber();

	/* Starts from a [ character and consumes up to and including the ] character. */
	bool ParseArray();

	/* Starts from an alphabetical character and consumes up to and including the last character of a sequence of alphabetical characters. */
	bool ParseLiteral();
	
	/* Calls any of the other Parse* Functions based on the character at the cursor. Returns the value it parsed as out parameter (can be nullptr). */
	bool ParseValue(Value* outValue = nullptr);

private:
	Value& GetCurrentValue();

	int m_Index = 0;
	Scope m_CurrentScope = Scope::OBJECT;
	std::string m_Key;
	std::string m_Source;
	std::vector<Object> m_Objects;
};



class ObjectBuilder {
public:
	ObjectBuilder();
	
	/* Writes a JSON key-value pair to the stream. */
	ObjectBuilder& WritePair(const std::string& inKey, const Value& inValue);

	/* Writes a JSON key-value pair to the stream directly from strings. */
	ObjectBuilder& WritePair(const std::string& inKey, const std::string& inValue);

	std::string Build();

private:
	void WriteValue(const Value& inValue, uint32_t inIndentLevel = 1);

	uint32_t m_CurrentLineLength = 0;
	std::stringstream m_Stream;
};



inline void ToJSONValue(JSON::Value& ioValue, std::string& inString) {
	ioValue.mType = JSON::ValueType::String;
	ioValue.mData.mString.mPtr = inString.c_str();
	ioValue.mData.mString.mLength = inString.size();
}

inline void ToJSONValue(JSON::Value& ioValue, float& inFloat) {
	ioValue.mType = JSON::ValueType::Number;
	ioValue.mData.mNumber = inFloat;
}

inline void ToJSONValue(JSON::Value& ioValue, bool& inBool) {
	ioValue.mType = JSON::ValueType::Bool;
	ioValue.mData.mBool = inBool;
}

inline void ToJSONValue(JSON::Value& ioValue, uint32_t& inInt) {
	ioValue.mType = JSON::ValueType::Number;
	ioValue.mData.mNumber = float(inInt);
}

inline void ToJSONValue(JSON::Value& ioValue, int& inInt) {
	ioValue.mType = JSON::ValueType::Number;
	ioValue.mData.mNumber = float(inInt);
}

inline void FromJSONValue(const JSON::Value& inValue, std::string& inString) {
	inString = inValue.As<JSON::String>().ToString();
}

inline void FromJSONValue(const JSON::Value& inValue, float& inFloat) {
	inFloat = inValue.As<float>();
}

inline void FromJSONValue(const JSON::Value& inValue, bool& inBool) {
	inBool = inValue.As<bool>();
}

inline void FromJSONValue(const JSON::Value& inValue, uint32_t& inInt) {
	inInt = uint32_t(inValue.As<float>());
}

inline void FromJSONValue(const JSON::Value& inValue, int& inInt) {
	inInt = int(inValue.As<float>());
}

template<glm::length_t L, typename T>
inline void ToJSONValue(JSON::Value& ioValue, glm::vec<L, T>& inVec) {
	ioValue.mType = JSON::ValueType::Array;
	ioValue.mArray.insert(ioValue.mArray.end(), L, JSON::Value(JSON::ValueType::Number));

	for (glm::length_t i = 0; i < L; i++)
		ToJSONValue(ioValue.mArray[i], inVec[i]);
}

template<glm::length_t L, typename T>
inline void FromJSONValue(const JSON::Value& ioValue, glm::vec<L, T>& inVec) {
	assert(ioValue.mArray.size() == L);

	for (glm::length_t i = 0; i < L; i++)
		FromJSONValue(ioValue.mArray[i], inVec[i]);
}


inline void FromJSONValue(const JSON::Value& inValue, glm::quat& inQuat) {
	for (glm::length_t i = 0; i < glm::quat::length(); i++)
		FromJSONValue(inValue.mArray[i], inQuat[i]);
}


inline void ToJSONValue(JSON::Value& ioValue, glm::quat& inQuat) {
	ioValue.mType = JSON::ValueType::Array;
	ioValue.mArray.insert(ioValue.mArray.end(), glm::quat::length(), JSON::Value(JSON::ValueType::Number));
	for (glm::length_t i = 0; i < glm::quat::length(); i++)
		ToJSONValue(ioValue.mArray[i], inQuat[i]);
}


inline void ToJSONValue(JSON::Value& ioValue, glm::mat4& inMatrix) {
	ioValue.mType = JSON::ValueType::Array;
	ioValue.mArray.insert(ioValue.mArray.end(), glm::mat4::length(), JSON::Value(JSON::ValueType::Array));

	for (glm::length_t i = 0; i < glm::mat4::length(); i++)
		ToJSONValue(ioValue.mArray[i], inMatrix[i]);
}

inline void FromJSONValue(const JSON::Value& ioValue, glm::mat4& inMatrix) {
	for (glm::length_t i = 0; i < glm::mat4::length(); i++)
		FromJSONValue(ioValue.mArray[i], inMatrix[i]);
}

template<unsigned int Count, typename T>
inline void FromJSONValue(const JSON::Value& ioValue, std::array<T, Count>& inArray) {
	for (uint64_t i = 0; i < inArray.size(); i++)
		FromJSONValue(ioValue.mArray[i], inArray[i]);
}


template<unsigned int Count, typename T>
inline void ToJSONValue(JSON::Value& ioValue, std::array<T, Count>& inArray) {
	ioValue.mType = JSON::ValueType::Array;
	ioValue.mArray.resize(inArray.size());
	for (uint64_t i = 0; i < inArray.size(); i++)
		ToJSONValue(ioValue.mArray[i], inArray[i]);
}


template<typename T>
inline void FromJSONValue(const JSON::Value& ioValue, std::vector<T>& inVector) {
	inVector.resize(ioValue.mArray.size());
	for (uint64_t i = 0; i < inVector.size(); i++)
		FromJSONValue(ioValue.mArray[i], inVector[i]);
}


template<typename T>
inline void ToJSONValue(JSON::Value& ioValue, std::vector<T>& inVector) {
	ioValue.mType = JSON::ValueType::Array;
	ioValue.mArray.resize(inVector.size());
	for (uint64_t i = 0; i < inVector.size(); i++)
		ToJSONValue(ioValue.mArray[i], inVector[i]);
}


template<typename T>
inline void FromJSONValue(const JSON::Value& ioValue, T* inPtr) {

}

template<typename T>
inline void ToJSONValue(JSON::Value& ioValue, T* inPtr) {

}

}