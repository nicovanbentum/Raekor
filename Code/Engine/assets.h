#pragma once

#include "dds.h"
#include "rtti.h"
#include "slice.h"

namespace RK {

class Asset
{
	RTTI_DECLARE_VIRTUAL_TYPE(Asset);

public:
	using Ptr = SharedPtr<Asset>;

	Asset() = default;
	Asset(const String& inPath) : m_Path(inPath) {}
	virtual ~Asset() = default;

	virtual bool Load() = 0;
	
	Path& GetPath() { return m_Path; }
	const Path& GetPath() const { return m_Path; }
	const String& GetPathAsString() { if (m_String.empty()) m_String = m_Path.string(); return m_String; }

	static std::string GetCachedPath(const String& inAssetPath, const char* inExtension)
	{
		return fs::relative(inAssetPath).replace_extension(inExtension).string().replace(0, 6, "Cached");
	}

protected:
	Path m_Path;
	String m_String;
};


class Assets
{
public:
	Assets();
	~Assets() = default;

	/* Request an asset given inPath. Will load the asset from disk if it wasn't already. This function is thread-safe (so you can load assets in parallel). */
	template<typename T> SharedPtr<T> GetAsset(const String& inPath);
	template<typename T> SharedPtr<T> GetAsset(const Path& inPath) { return GetAsset<T>(inPath.string()); }

	bool ReleaseAsset(const String& inPath);
	bool ContainsAsset(const String& inPath) const { return m_Assets.contains(inPath); }

	/* Releases any assets that are no longer referenced elsewhere. */
	void ReleaseUnreferenced();

private:
	std::mutex m_Mutex;
	HashMap<String, Asset::Ptr> m_Assets;
};


class TextureAsset : public Asset
{
public:
	RTTI_DECLARE_VIRTUAL_TYPE(TextureAsset);

	using Ptr = SharedPtr<TextureAsset>;

	TextureAsset() = default;
	TextureAsset(const String& inPath) : Asset(inPath) {}
	virtual ~TextureAsset() = default;

	[[deprecated]] bool Save();

	virtual bool Load() override;
	static String Convert(const String& inPath);
	static String GetCachedPath(const String& inPath) { return Asset::GetCachedPath(inPath, ".dds"); }

	bool IsExtendedDX10() const { return m_IsExtendedDX10; }

	uint32_t GetDataSize() const { return m_Data.size() - m_IsExtendedDX10 ? sizeof(DDSFileInfoExtended) : sizeof(DDSFileInfo); }
	const char* const GetData() const { return m_Data.data() + int(m_IsExtendedDX10 ? sizeof(DDSFileInfoExtended) : sizeof(DDSFileInfo)); }

	DDS_HEADER* GetHeader() { return reinterpret_cast<DDS_HEADER*>( m_Data.data() + sizeof(uint32_t) ); }
	const DDS_HEADER* GetHeader() const { return reinterpret_cast<const DDS_HEADER*>( m_Data.data() + sizeof(uint32_t) ); }

	DDS_HEADER_DXT10* GetHeaderDXT10() { return reinterpret_cast<DDS_HEADER_DXT10*>( m_Data.data() + sizeof(uint32_t) + sizeof(DDS_HEADER)); }
	const DDS_HEADER_DXT10* GetHeaderDXT10() const { return reinterpret_cast<const DDS_HEADER_DXT10*>( m_Data.data() + sizeof(uint32_t) + sizeof(DDS_HEADER) ); }

private:
	Array<char> m_Data;
	uint32_t m_Texture = 0;
	bool m_IsExtendedDX10 = false;
};



class ScriptAsset : public Asset
{
public:
	RTTI_DECLARE_VIRTUAL_TYPE(ScriptAsset);

	using Ptr = SharedPtr<ScriptAsset>;

	ScriptAsset() = default;
	ScriptAsset(const String& inPath) : Asset(inPath), m_TempPath(inPath) { m_TempPath.replace_extension(".temp.dll"); }
	virtual ~ScriptAsset();

	virtual bool Load() override;
	static String Convert(const String& inPath);
	static String GetCachedPath(const String& inPath) { return Asset::GetCachedPath(inPath, ".dll"); }

	void EnumerateSymbols();
	Slice<String> GetRegisteredTypes() const { return Slice(m_RegisteredTypes); }

private:
	Path m_TempPath;
	void* m_HModule = nullptr;
	Array<String> m_RegisteredTypes;
};


} // namespace raekor


namespace RK {

template<typename T>
SharedPtr<T> Assets::GetAsset(const String& inPath)
{
	{
		std::scoped_lock lock(m_Mutex);

		// if it already has an asset pointer it means some other thread added it,
		// so just return whats there, the thread that added it is responsible for loading.
		if (auto asset = m_Assets.find(inPath); asset != m_Assets.end())
			return std::static_pointer_cast<T>( asset->second );
		// if there is no asset pointer already and the file path exists on disk, insert it
		else if (fs::exists(inPath) && fs::is_regular_file(inPath))
			m_Assets.insert(std::make_pair(inPath, std::shared_ptr<Asset>(new T(inPath))));
		else
			// can't load anything if it doesn't exist on disk
			return nullptr;
	}
	// only get here if this thread created the asset pointer, try to load it.
	// if load succeeds we return the asset pointer
	const auto& asset = m_Assets.at(inPath);
	if (asset->Load())
		return std::static_pointer_cast<T>( asset );
	else
	{
		// if load failed, lock -> remove asset pointer -> return nullptr
		ReleaseAsset(inPath);
		return nullptr;
	}
}

}