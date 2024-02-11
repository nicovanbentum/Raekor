#pragma once

#include "slice.h"
#include "rmath.h"

namespace Raekor {

enum ESerializeType
{
	SERIALIZE_NONE = 0 << 0,
	SERIALIZE_JSON = 1 << 0,
	SERIALIZE_BINARY = 1 << 1,
	SERIALIZE_ALL = SERIALIZE_JSON | SERIALIZE_BINARY
};

enum EPakCompressionType
{
	COMPRESS_NONE,
	COMPRESS_LZ4,
	COMPRESS_GDEFLATE
};

constexpr auto PAK_VERSION = 1u;

struct PakHeader
{
	uint32_t mMagicNumber = 'PAK';
	uint32_t mVersionNumber = PAK_VERSION;
	uint32_t mContentVersionNumber = 1;
	uint32_t mOffset = 0;
	uint32_t mEntryCount = 0;
};

struct PakEntry
{
	char mNameOrPath[255];
	EPakCompressionType mCompressionType;
	uint32_t mOffset = 0;
	uint32_t mSize = 0;
	uint32_t mUncompressedSize = 0;
};



template<typename T>
void ReadFileData(File& ioFile, T& ioData) { ioFile.read((char*)&ioData, sizeof(T)); }
template<typename T>
inline void WriteFileData(File& ioFile, const T& inData) { ioFile.write((const char*)&inData, sizeof(T)); }


template<typename T>
void WriteFileSlice(File& ioFile, Slice<T> inData) { ioFile.write((const char*)inData.GetPtr(), inData.Length() * sizeof(T)); }
template<typename T>
void ReadFileSlice(File& ioFile, Slice<T> inData) { ioFile.read((char*)inData.GetPtr(), inData.Length() * sizeof(T)); }


template<typename T>
inline void WriteFileBinary(File& ioFile, const T& inData) { WriteFileData(ioFile, inData); }
template<typename T>
inline void ReadFileBinary(File& ioFile, T& ioData) { ReadFileData(ioFile, ioData); }


inline void ReadFileBinary(File& ioFile, Path& ioData)
{
	std::string value;
	ReadFileBinary(ioFile, value);
	ioData = std::move(value);
}
inline void WriteFileBinary(File& ioFile, const Path& inData)
{
	WriteFileBinary(ioFile, inData.string());
}


inline void ReadFileBinary(File& ioFile, std::string& ioData)
{
	size_t size = 0;
	ReadFileData(ioFile, size);
	ioData.resize(size);
	ReadFileSlice(ioFile, Slice<char>(ioData));
}
inline void WriteFileBinary(File& ioFile, const std::string& inData)
{
	WriteFileData(ioFile, inData.size());
	WriteFileSlice(ioFile, Slice<char>(inData));
}


template<typename T>
inline void WriteFileBinary(File& ioFile, const std::vector<T>& inData)
{
	WriteFileData(ioFile, inData.size());
	WriteFileSlice(ioFile, Slice(inData));
}
template<typename T>
inline void ReadFileBinary(File& ioFile, std::vector<T>& ioData)
{
	size_t size = 0;
	ReadFileData(ioFile, size);
	ioData.resize(size);

	if constexpr (std::is_trivial_v<T>)
	{
		ReadFileSlice(ioFile, Slice(ioData));
	}
	else
	{
		for (size_t i = 0; i < size; i++)
			ReadFileBinary(ioFile, ioData[i]);
	}
}

template<typename T>
concept HasRTTI = requires ( T t ) { t.GetRTTI(); };

template<typename T> requires HasRTTI<T>
inline void ReadFileBinary(File& ioFile, T& ioData)
{
	auto& rtti = gGetRTTI<T>();
	for (const auto& member : rtti)
	{
		if (member->GetSerializeType() & SERIALIZE_BINARY)
			member->FromBinary(ioFile, &ioData);
	}
}
template<typename T> requires HasRTTI<T>
inline void WriteFileBinary(File& ioFile, const T& inData)
{
	auto& rtti = gGetRTTI<T>();
	for (const auto& member : rtti)
	{
		if (member->GetSerializeType() & SERIALIZE_BINARY)
			member->ToBinary(ioFile, &inData);
	}
}

} // Raekor

