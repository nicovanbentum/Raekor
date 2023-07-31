#pragma once

#include "util.h"

namespace Raekor::JSON {

/* Advance to the first token right after the end of the token at inIdx */
int SkipToken(Slice<jsmntok_t> inTokens, int inIdx);

inline void ToJSONValue(std::string& inJSON, std::string& inString) {
	inJSON = inString;
}

inline void ToJSONValue(std::string& inJSON, float& inFloat) {
	inJSON = std::to_string(inFloat);
}

inline void ToJSONValue(std::string& inJSON, bool& inBool) {
	inJSON = inBool ? "true" : "false";
}

inline void ToJSONValue(std::string& inJSON, uint32_t& inInt) {
	inJSON = std::to_string(inInt);
}

inline void ToJSONValue(std::string& inJSON, int& inInt) {
	inJSON = std::to_string(inInt);
}

inline uint32_t FromJSONValue(const std::string& inJSON, Slice<jsmntok_t> inTokens, std::string& ioString) {
	assert(inTokens.Length() == 1);
	ioString = std::string(&inJSON[inTokens[0].start], inTokens[0].end - inTokens[0].start);
	return 1;
}

inline uint32_t FromJSONValue(const std::string& inJSON, Slice<jsmntok_t> inTokens, float& ioFloat) {
	assert(inTokens.Length() == 1);
	std::from_chars(&inJSON[inTokens[0].start], &inJSON[inTokens[0].end], ioFloat);
	return 1;
}

inline uint32_t FromJSONValue(const std::string& inJSON, Slice<jsmntok_t> inTokens, bool& inBool) {
	assert(inTokens.Length() == 1);
	inBool = inJSON[inTokens[0].start] == 't';
	return 1;
}

inline uint32_t FromJSONValue(const std::string& inJSON, Slice<jsmntok_t> inTokens, uint32_t& ioInt) {
	assert(inTokens.Length() == 1);
	std::from_chars(&inJSON[inTokens[0].start], &inJSON[inTokens[0].end], ioInt);
	return 1;
}

inline uint32_t FromJSONValue(const std::string& inJSON, Slice<jsmntok_t> inTokens, int& ioInt) {
	assert(inTokens.Length() == 1);
	std::from_chars(&inJSON[inTokens[0].start], &inJSON[inTokens[0].end], ioInt);
	return 1;
}

template<glm::length_t L, typename T>
inline void ToJSONValue(std::string& ioJSON, glm::vec<L, T>& inVec) {
	// TODO
}

template<glm::length_t L, typename T>
inline uint32_t FromJSONValue(const std::string& inJSON, Slice<jsmntok_t> inTokens, glm::vec<L, T>& inVec) {
	return 0; // TODO
}


inline uint32_t FromJSONValue(const std::string& inJSON, Slice<jsmntok_t> inTokens, glm::quat& inQuat) {
	return 0; // TODO
}


inline void ToJSONValue(std::string& inJSON, glm::quat& inQuat) {
	// TODO
}


inline void ToJSONValue(std::string& inJSON, glm::mat4& inMatrix) {
	// TODO
}

inline uint32_t FromJSONValue(const std::string& inJSON, Slice<jsmntok_t> inTokens, glm::mat4& inMatrix) {
	return 0; // TODO
}

template<unsigned int Count, typename T>
inline uint32_t FromJSONValue(const std::string& inJSON, Slice<jsmntok_t> inTokens, std::array<T, Count>& inArray) {
	return 0; // TODO
}


template<unsigned int Count, typename T>
inline void ToJSONValue(std::string& inJSON, std::array<T, Count>& inArray) {
	// TODO
}


template<typename T>
inline uint32_t FromJSONValue(const std::string& inJSON, Slice<jsmntok_t> inTokens, std::vector<T>& inVector) {
	return 0; // TODO
}


template<typename T>
inline void ToJSONValue(std::string& inJSON, std::vector<T>& inVector) {
	// TODO
}


inline uint32_t FromJSONValue(const std::string& inJSON, Slice<jsmntok_t> inTokens, Path& inPath) {
	std::string value;
	const auto result = FromJSONValue(inJSON, inTokens, value);
	inPath = Path(value);
	return result;
}


inline void ToJSONValue(std::string& ioJSON, Path& inPath) {
	std::string value = inPath.string();
	ToJSONValue(ioJSON, value);
}

template<typename T>
inline uint32_t FromJSONValue(const std::string& inJSON, Slice<jsmntok_t> inTokens, T* inPtr) {
	return 0;
}

template<typename T>
inline void ToJSONValue(std::string& inJSON, T* inPtr) {
	return 0;
}

template<typename T>
inline void ToJSONValue(std::string& inJSON, T& inMember) {
	auto& rtti = gGetRTTI<T>();
	for (uint32_t i = 0; i < rtti.GetMemberCount(); i++) {
		rtti.GetMember(i)->ToJSON(inJSON, &inMember);
	}
}

template<typename T>
inline uint32_t FromJSONValue(const std::string& inJSON, Slice<jsmntok_t> inTokens, T& inMember) {
	if (inTokens.Length() < 2u) // empty array or empty object?
		return 0;

	auto token_index = 0;
	const auto& object_token = inTokens[token_index]; // index to object
	
	auto& rtti = gGetRTTI<T>();

	if (object_token.type == JSMN_OBJECT) {
		for (auto key_index = 0; key_index < object_token.size; key_index++) {
			const auto& key_token = inTokens[++token_index];
			const auto key = std::string(&inJSON[key_token.start], key_token.end - key_token.start);

			token_index++; // increment index to value
			if (const auto member = rtti.GetMember(key)) {
				const auto& value_token = inTokens[token_index];

				// Object and Array values can span multiple tokens
				if (value_token.type == JSMN_OBJECT || value_token.type == JSMN_ARRAY) {
					auto token_index_end = SkipToken(inTokens, token_index);
					if (token_index_end == -1) // hit EOF?
						token_index_end = inTokens.Length() - 1;

					const auto value_tokens = Slice(&inTokens[token_index], token_index_end - token_index);

					const auto skipped_tokens = member->FromJSON(inJSON, value_tokens, &inMember);

					SkipToken(inTokens, token_index);
				}
				else {
					// Primitives, Strings and null are just 1 token
					member->FromJSON(inJSON, Slice(&inTokens[token_index]), &inMember);
				}
			}
			else {	// key is not a valid member, skip the value to get to the next key
				token_index = SkipToken(inTokens, token_index);
			}
		}


	}
	else if (inTokens[0].type == JSMN_ARRAY) {
		// TODO
	}
	else
		return 0;

	return token_index;
}

}