#pragma once

#include "dds.h"
#include "rtti.h"

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

	static String GetCachedPath(const String& inAssetPath, const char* inExtension)
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
	/* You might get an asset that's still loading on another thread though... REWORK TIME! */
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

	const size_t GetDataSize() const { return m_Data.size(); }
	const uint8_t* const GetData() const { return m_Data.data(); }

private:
	Array<uint8_t> m_Data;
	uint32_t m_Texture = 0;
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
	const Array<String>& GetRegisteredTypes() const { return m_RegisteredTypes; }

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
    SharedPtr<T> asset_ptr = nullptr;

	{
		std::scoped_lock lock(m_Mutex);

		// if it already has an asset pointer it means some other thread added it,
		// so just return whats there, the thread that added it is responsible for loading.
        if (auto asset = m_Assets.find(inPath); asset != m_Assets.end())
        {
			return std::static_pointer_cast<T>( asset->second );
        }
		// if there is no asset pointer already and the file path exists on disk, insert it
        else if (fs::exists(inPath) && fs::is_regular_file(inPath))
        {
            asset_ptr = std::make_shared<T>(inPath);
			m_Assets.insert(std::make_pair(inPath, asset_ptr));
        }
        else
        {
			// can't load anything if it doesn't exist on disk
			return nullptr;
        }
	}
	// only get here if this thread created the asset pointer, try to load it.
	// if load succeeds we return the asset pointer
	if (asset_ptr->Load())
		return asset_ptr;
	else
	{
		// if load failed, lock -> remove asset pointer -> return nullptr
		ReleaseAsset(inPath);
		return nullptr;
	}
}

}