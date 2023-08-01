#pragma once

#include "util.h"
#include "rmath.h"

namespace Raekor::JSON {

class JSONData {
public:
	JSONData() = default;
	JSONData(const Path& inPath);

	bool IsEmpty() const { return m_StrBuffer.empty() || m_Tokens.empty(); }
	uint32_t GetTokenCount() const { return m_Tokens.size(); }
	Slice<jsmntok_t> GetTokens() const { return Slice(m_Tokens); }

	bool IsKeyObjectPair(uint32_t inTokenIdx);
	uint32_t SkipToken(uint32_t inTokenIdx);

	const jsmntok_t&	GetToken(uint32_t inTokenIdx) { return m_Tokens[inTokenIdx]; }
	const std::string&	GetString(uint32_t inTokenIdx) { return m_Strings[inTokenIdx]; }
	double				GetPrimitive(uint32_t inTokenIdx) { return m_Primitives[inTokenIdx]; }

public:
	// Generic T and primitive types
	template<typename T> 
	uint32_t GetTokenToValue(uint32_t inTokenIdx, T& inValue);
	uint32_t GetTokenToValue(uint32_t inTokenIdx, int& inValue);
	uint32_t GetTokenToValue(uint32_t inTokenIdx, bool& inValue);
	uint32_t GetTokenToValue(uint32_t inTokenIdx, float& inValue);
	uint32_t GetTokenToValue(uint32_t inTokenIdx, uint32_t& inValue);
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

	// Other
	uint32_t GetTokenToValue(uint32_t inTokenIdx, Path& inValue);

private:
	std::string m_StrBuffer;
	std::vector<jsmntok_t> m_Tokens;
	std::vector<double> m_Primitives;
	std::vector<std::string> m_Strings;
};



template<typename T>
inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIdx, T& inValue) {
	const auto& object_token = m_Tokens[inTokenIdx]; // index to object
	// T& can only deal with JSMN_OBJECT
	if (object_token.type != JSMN_OBJECT)
		return SkipToken(inTokenIdx);
	
	auto& rtti = gGetRTTI<T>();
	inTokenIdx++; // increment index to first key (name of the first class member)

	for (auto key_index = 0; key_index < object_token.size; key_index++) {
		 auto key_string = GetString(inTokenIdx); // member name

		 inTokenIdx++; // increment index to value
		if (const auto member = gGetRTTI<T>().GetMember(key_string)) {
			// parse the current value, increment the token index by how many we have parsed
			inTokenIdx = member->FromJSON(*this, inTokenIdx, &inValue);
		}
		else { // key is not a valid member, skip the value to get to the next key
			inTokenIdx = SkipToken(inTokenIdx);
		}
	}

	return inTokenIdx;
}


inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, int& ioInt) {
	ioInt = GetPrimitive(inTokenIndex++); return inTokenIndex;
}

inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, bool& ioBool) {
	ioBool = GetPrimitive(inTokenIndex++); return inTokenIndex;
}

inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, float& ioFloat) {
	ioFloat = GetPrimitive(inTokenIndex++); return inTokenIndex;
}

inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, uint32_t& ioInt) {
	ioInt = GetPrimitive(inTokenIndex++); return inTokenIndex;
}

inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, std::string& ioString) {
	ioString = GetString(inTokenIndex++); return inTokenIndex;
}

template<glm::length_t L, typename T>
inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, glm::vec<L, T>& inVec) {
	inVec = gFromString<L, T>(GetString(inTokenIndex++)); return inTokenIndex;
}

inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, glm::quat& inQuat) {
	return 0; // TODO
}

template<glm::length_t C, glm::length_t R, typename T>
inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, glm::mat<C, R, T>& inMatrix) {
	inMatrix = gFromString<C, R, T>(GetString(inTokenIndex++)); return inTokenIndex;
}


template<typename T>
inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, std::vector<T>& inVector) {
	return 0; // TODO
}

template<typename T, uint32_t N>
inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, std::array<T, N>& inArray) {
	return 0; // TODO
}


inline uint32_t JSONData::GetTokenToValue(uint32_t inTokenIndex, Path& inPath) {
	std::string value;
	const auto result = GetTokenToValue(inTokenIndex, value);
	inPath = Path(value);
	return result;
}



template<typename T>
inline void GetValueToJSON(std::string& inJSON, T& inMember) {
	inJSON += "{";
	auto& rtti = gGetRTTI<T>();
	for (uint32_t i = 0; i < rtti.GetMemberCount(); i++) {
		rtti.GetMember(i)->ToJSON(inJSON, &inMember);

		if (i != rtti.GetMemberCount() - 1)
			inJSON += ",";
	}

	inJSON += "}";
}


inline void GetValueToJSON(std::string& inJSON, int& inInt) {
	inJSON += std::to_string(inInt);
}

inline void GetValueToJSON(std::string& inJSON, float& inFloat) {
	inJSON += std::to_string(inFloat);
}

inline void GetValueToJSON(std::string& inJSON, bool& inBool) {
	inJSON += inBool ? "true" : "false";
}

inline void GetValueToJSON(std::string& inJSON, uint32_t& inInt) {
	inJSON += std::to_string(inInt);
}

inline void GetValueToJSON(std::string& inJSON, std::string& inString) {
	inJSON += "\"" + inString + "\"";
}

template<glm::length_t L, typename T>
inline void GetValueToJSON(std::string& ioJSON, glm::vec<L, T>& inVec) {
	ioJSON += "\"" + gToString(inVec) + "\"";
}

inline void GetValueToJSON(std::string& ioJSON, glm::quat& inQuat) {
	ioJSON += "\"" + glm::to_string(inQuat) + "\"";

}

template<glm::length_t C, glm::length_t R, typename T>
inline void GetValueToJSON(std::string& ioJSON, glm::mat<C, R, T>& inMatrix) {
	ioJSON += "\"" + gToString(inMatrix) + "\"";
}


template<typename T>
inline void GetValueToJSON(std::string& ioJSON, std::vector<T>& inVector) {
	// TODO
}

template<unsigned int Count, typename T>
inline void GetValueToJSON(std::string& ioJSON, std::array<T, Count>& inArray) {
	// TODO
}


inline void GetValueToJSON(std::string& ioJSON, Path& inPath) {
	std::string value = inPath.string();
	GetValueToJSON(ioJSON, value);
}

} // Raekor::JSON