#pragma once

#include "dds.h"

namespace Raekor {

class Asset {
    friend class Assets;

public:
    using Ptr = Asset*;

    Asset() = default;
    Asset(const std::string& inPath) : m_Path(inPath) {}
    virtual ~Asset() = default;

    template<class Archive>
    void serialize(Archive& inArchive) { inArchive(m_Path); }

    Path& GetPath() { return m_Path; }
    const Path& GetPath() const { return m_Path; }

    // ONLY USE FOR FIXING UP OLD SCENES WHERE m_Path WASN'T SERIALIZED!!
    void SetPath(const std::string& inPath) { m_Path = inPath; }

    virtual bool Load(const std::string& inPath) = 0;

protected:

    Path m_Path;
};


class Assets : public std::unordered_map<std::string, std::shared_ptr<Asset>> {
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

    [[nodiscard]] inline bool Contains(const std::string& inPath) const { return find(inPath) != end(); }

    void Release(const std::string& inPath);

    /* Releases any assets that are no longer referenced elsewhere. */
    void ReleaseUnreferenced();

private:
    std::mutex m_Mutex;
};


class TextureAsset : public Asset {
public:
    using Ptr = std::shared_ptr<TextureAsset>;

    TextureAsset() = default;
    TextureAsset(const std::string& inPath) : Asset(inPath) {}
    virtual ~TextureAsset() = default;

    static std::string sConvert(const std::string& inPath);
    virtual bool Load(const std::string& inPath) override;

    template<class Archive> 
    void serialize(Archive& ioArchive) { ioArchive(m_Data); }

    char* const         GetData();
    const DDS_HEADER*   GetHeader();
    uint32_t            GetDataSize() const;

private:
    std::vector<char> m_Data;
};


class ShaderAsset : public Asset {
public:
    using Ptr = std::shared_ptr<ShaderAsset>;

    ShaderAsset() = default;
    ShaderAsset(const std::string& inPath) : Asset(inPath) {}
    virtual ~ShaderAsset() = default;

    static std::string sConvert(const std::string& inPath);
    virtual bool Load(const std::string& inPath) override;

    template<class Archive>
    void serialize(Archive& ioArchive) {  }

private:
    int f;
};


class ScriptAsset : public Asset {
public:
    using Ptr = std::shared_ptr<ScriptAsset>;

    ScriptAsset() = default;
    ScriptAsset(const std::string& inPath) : Asset(inPath) {}
    virtual ~ScriptAsset();

    static Path sConvert(const Path& inPath);
    virtual bool Load(const std::string& inPath) override;

    template<class Archive> 
    void serialize(Archive& ioArchive) {}

    void EnumerateSymbols();
    HMODULE GetModule() { return m_HModule; }

private:
    HMODULE m_HModule;
};


} // namespace raekor


CEREAL_REGISTER_TYPE(Raekor::Asset);
CEREAL_REGISTER_TYPE(Raekor::ScriptAsset);
CEREAL_REGISTER_TYPE(Raekor::TextureAsset);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Raekor::Asset, Raekor::ScriptAsset);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Raekor::Asset, Raekor::TextureAsset);


namespace Raekor {

template<typename T>
std::shared_ptr<T> Assets::GetAsset(const std::string& inPath) {
    {
        auto lock = std::scoped_lock(m_Mutex);

        // if it already has an asset pointer it means some other thread added it,
        // so just return whats there, the thread that added it is responsible for loading.
        if (auto asset = find(inPath); asset != end())
            return std::static_pointer_cast<T>(asset->second);
        // if there is no asset pointer already and the file path exists on disk, insert it
        else if (FileSystem::exists(inPath))
            insert(std::make_pair(inPath, std::shared_ptr<Asset>(new T(inPath))));
        else
            // can't load anything if it doesn't exist on disk
            return nullptr;
    }
    // only get here if this thread created the asset pointer, try to load it.
    // if load succeeds we return the asset pointer
    const auto& asset = at(inPath);
    if (asset->Load(inPath))
        return std::static_pointer_cast<T>(asset);
    else {
        // if load failed, lock -> remove asset pointer -> return nullptr
        Release(inPath);
        return nullptr;
    }
}

}