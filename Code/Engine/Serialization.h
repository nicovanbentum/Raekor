#pragma once

#include "slice.h"

namespace RK {

enum ESerializeType
{
	SERIALIZE_NONE = 0 << 0,
	SERIALIZE_JSON = 1 << 0,
	SERIALIZE_BINARY = 1 << 1,
	SERIALIZE_ALL = SERIALIZE_JSON | SERIALIZE_BINARY
};

enum ECompressionType
{
	COMPRESS_NONE,
	COMPRESS_LZ4
};


struct SceneHeader
{
	static constexpr uint32_t sVersion = 2;
	static constexpr uint64_t sMagicNumber = 'RKSC';

	uint32_t Version;
	uint64_t MagicNumber;
	uint64_t IndexTableStart;
	uint64_t IndexTableCount;
};

struct SceneTable
{
	uint32_t Hash;
	uint32_t Size;
	uint32_t Start;
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
	auto size = inData.size();
	WriteFileData(ioFile, size);

	if constexpr (std::is_trivially_copyable_v<T>)
	{
		WriteFileSlice(ioFile, Slice(inData));
	}
	else
	{
		for (size_t i = 0; i < size; i++)
			WriteFileBinary(ioFile, inData[i]);
	}
}


template<typename K, typename V>
inline void WriteFileBinary(File& ioFile, const std::unordered_map<K, V>& inData)
{
	auto size = inData.size();
	WriteFileBinary(ioFile, size);

	for (const auto& [key, value] : inData)
	{
		WriteFileBinary(ioFile, key);
		WriteFileBinary(ioFile, value);
	}
}


template<typename K, typename V>
inline void ReadFileBinary(File& ioFile, std::unordered_map<K, V>& inData)
{
	size_t size = 0;
	ReadFileBinary(ioFile, size);

	for (size_t i = 0; i < size; i++)
	{
		K key;
		ReadFileBinary(ioFile, key);
		ReadFileBinary(ioFile, inData[key]);
	}
}

template<typename T>
inline void ReadFileBinary(File& ioFile, std::vector<T>& ioData)
{
	size_t size = 0;
	ReadFileData(ioFile, size);
	ioData.resize(size);

	if constexpr (std::is_trivially_copyable_v<T>)
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

void RunTestsSerialCpp();

} // Raekor

