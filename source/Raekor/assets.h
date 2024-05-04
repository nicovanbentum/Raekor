#pragma once

#include "dds.h"
#include "rtti.h"
#include "slice.h"

namespace RK {

class Asset
{
	RTTI_DECLARE_VIRTUAL_TYPE(Asset);

	friend class Assets;

public:
	using Ptr = std::shared_ptr<Asset>;

	Asset() = default;
	Asset(const std::string& inPath) : m_Path(inPath) {}
	virtual ~Asset() = default;

	Path& GetPath() { return m_Path; }
	const Path& GetPath() const { return m_Path; }

	// ONLY USE FOR FIXING UP OLD SCENES WHERE m_Path WASN'T SERIALIZED!!
	[[deprecated]] void SetPath(const std::string& inPath) { m_Path = inPath; }

	virtual bool Load() = 0;

	static std::string sAssetsToCachedPath(const std::string& inAssetPath, const char* inExtension)
	{
		return fs::relative(inAssetPath).replace_extension(inExtension).string().replace(0, 6, "cached");
	}

protected:
	Path m_Path;
};


class Assets : public std::unordered_map<std::string, Asset::Ptr>
{
public:
	Assets();
	Assets(const Assets&) = delete;
	Assets(const Assets&&) = delete;
	Assets& operator=(Assets&) = delete;
	Assets& operator=(Assets&&) = delete;

	/* Request an asset given inPath. Will load the asset from disk if it wasn't already. This function is thread-safe (so you can load assets in parallel). */
	template<typename T>
	std::shared_ptr<T> GetAsset(const std::string& inPath);

	template<typename T>
	std::shared_ptr<T> GetAsset(const Path& inPath) { return GetAsset<T>(inPath.string()); }

	[[nodiscard]] inline bool Contains(const std::string& inPath) const { return contains(inPath); }

	void Release(const std::string& inPath);

	/* Releases any assets that are no longer referenced elsewhere. */
	void ReleaseUnreferenced();

private:
	std::mutex m_Mutex;
};


class TextureAsset : public Asset
{
public:
	RTTI_DECLARE_VIRTUAL_TYPE(TextureAsset);

	using Ptr = std::shared_ptr<TextureAsset>;

	TextureAsset() = default;
	TextureAsset(const std::string& inPath) : Asset(inPath) {}
	virtual ~TextureAsset() = default;

	static std::string sConvert(const std::string& inPath);
	virtual bool Load() override;

	// ONLY USE FOR FIXING UP BAD ASSETS!!
	[[deprecated]] bool Save();

	static std::string sAssetsToCachedPath(const std::string& inAssetPath) { return Asset::sAssetsToCachedPath(inAssetPath, ".dds"); }

	bool IsExtendedDX10() const { return m_IsExtendedDX10; }

	uint32_t GetDataSize() const { return m_Data.size() - m_IsExtendedDX10 ? sizeof(DDSFileInfoExtended) : sizeof(DDSFileInfo); }
	const char* const GetData() const { return m_Data.data() + int(m_IsExtendedDX10 ? sizeof(DDSFileInfoExtended) : sizeof(DDSFileInfo)); }

	DDS_HEADER* GetHeader() { return reinterpret_cast<DDS_HEADER*>( m_Data.data() + sizeof(uint32_t) ); }
	const DDS_HEADER* GetHeader() const { return reinterpret_cast<const DDS_HEADER*>( m_Data.data() + sizeof(uint32_t) ); }

	DDS_HEADER_DXT10* GetHeaderDXT10() { return reinterpret_cast<DDS_HEADER_DXT10*>( m_Data.data() + sizeof(uint32_t) + sizeof(DDS_HEADER)); }
	const DDS_HEADER_DXT10* GetHeaderDXT10() const { return reinterpret_cast<const DDS_HEADER_DXT10*>( m_Data.data() + sizeof(uint32_t) + sizeof(DDS_HEADER) ); }

private:
	uint32_t m_Texture = 0;
	Array<char> m_Data;
	bool m_IsExtendedDX10 = false;
};



class ScriptAsset : public Asset
{
public:
	RTTI_DECLARE_VIRTUAL_TYPE(TextureAsset);

	using Ptr = std::shared_ptr<ScriptAsset>;

	ScriptAsset() = default;
	ScriptAsset(const std::string& inPath) : Asset(inPath), m_TempPath(inPath) { m_TempPath.replace_extension(".temp.dll"); }
	virtual ~ScriptAsset();

	static Path sConvert(const Path& inPath);
	virtual bool Load() override;

	void EnumerateSymbols();


	Slice<std::string> GetRegisteredTypes() const { return Slice(m_RegisteredTypes); }

private:
	Path m_TempPath;
	void* m_HModule;
	Array<std::string> m_RegisteredTypes;
};


} // namespace raekor


namespace RK {

template<typename T>
std::shared_ptr<T> Assets::GetAsset(const std::string& inPath)
{
	{
		std::scoped_lock lock(m_Mutex);

		// if it already has an asset pointer it means some other thread added it,
		// so just return whats there, the thread that added it is responsible for loading.
		if (auto asset = find(inPath); asset != end())
			return std::static_pointer_cast<T>( asset->second );
		// if there is no asset pointer already and the file path exists on disk, insert it
		else if (fs::exists(inPath) && fs::is_regular_file(inPath))
			insert(std::make_pair(inPath, std::shared_ptr<Asset>(new T(inPath))));
		else
			// can't load anything if it doesn't exist on disk
			return nullptr;
	}
	// only get here if this thread created the asset pointer, try to load it.
	// if load succeeds we return the asset pointer
	const auto& asset = at(inPath);
	if (asset->Load())
		return std::static_pointer_cast<T>( asset );
	else
	{
		// if load failed, lock -> remove asset pointer -> return nullptr
		Release(inPath);
		return nullptr;
	}
}

}