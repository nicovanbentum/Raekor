#pragma once

#include "dds.h"

namespace Raekor {

class Asset {
    friend class Assets;

public:
    Asset() = default;
    Asset(const std::string& path) : m_Path(path) {}
    virtual ~Asset() = default;

    template<class Archive>
    void serialize(Archive& archive) { archive(m_Path); }

    fs::path GetPath() { return m_Path; }
    const fs::path& GetPath() const { return m_Path; }

    virtual bool Load(const std::string& path) = 0;

protected:
    fs::path m_Path;
};


class Assets : public std::unordered_map<std::string, std::shared_ptr<Asset>> {
public:
    Assets();
    Assets(const Assets&) = delete;
    Assets(const Assets&&) = delete;
    Assets& operator=(Assets&) = delete;
    Assets& operator=(Assets&&) = delete;

    template<typename T>
    std::shared_ptr<T> Get(const std::string& path);
    void Release(const std::string& path);

    /* Releases any assets that are no longer referenced elsewhere. */
    void ReleaseUnreferenced();

private:
    std::mutex m_LoadMutex;
    std::mutex m_ReleaseMutex;
};


template<typename T>
std::shared_ptr<T> Assets::Get(const std::string& path) {
    if (!fs::exists(path))
        return nullptr;
    {
        std::scoped_lock lk(m_LoadMutex);

        // if it already has an asset pointer it means some other thread added it,
        // so just return whats there, the thread that added it is responsible for loading.
        if (auto asset = find(path); asset != end())
            return std::static_pointer_cast<T>(asset->second);
        else
            insert(std::make_pair(path, std::shared_ptr<Asset>(new T(path))));
    }
    // only get here if this thread created the asset pointer, try to load it.
    // if load succeeds we return the asset pointer
    if (at(path)->Load(path))
        return std::static_pointer_cast<T>(at(path));
    else {
        // if load failed, lock -> remove asset pointer -> return nullptr
        erase(path);
        return nullptr;
    }
}


class TextureAsset : public Asset {
public:
    using Ptr = std::shared_ptr<TextureAsset>;

    TextureAsset() = default;
    TextureAsset(const std::string& filepath) : Asset(filepath) {}
    virtual ~TextureAsset() = default;

    static std::string sConvert(const std::string& path);
    virtual bool Load(const std::string& path) override;

    template<class Archive> 
    void serialize(Archive& archive) { archive(m_Data); }

    char* const         GetData();
    const DDS_HEADER*   GetHeader();
    uint32_t            GetDataSize() const;

private:
    std::vector<char> m_Data;
};


class ScriptAsset : public Asset {
public:
    using Ptr = std::shared_ptr<ScriptAsset>;

    ScriptAsset() = default;
    ScriptAsset(const std::string& filepath) : Asset(filepath) {}
    virtual ~ScriptAsset();

    static std::string sConvert(const std::string& filepath);
    virtual bool Load(const std::string& filepath) override;

    template<class Archive> 
    void serialize(Archive& archive) {}

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
