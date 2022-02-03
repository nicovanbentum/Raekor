#pragma once

#include "dds.h"


namespace Raekor {

class Asset {
public:
    Asset() = default;
    Asset(const std::string& filepath) : m_path(filepath) {}
    virtual ~Asset() = default;

    inline fs::path path() { return m_path; }

    virtual bool load(const std::string& inpath) = 0;

public:
    fs::path m_path;
};



class Assets : public std::unordered_map<std::string, std::shared_ptr<Asset>> {
public:
    Assets();
    Assets(const Assets&) = delete;
    Assets(const Assets&&) = delete;
    Assets& operator=(Assets&) = delete;
    Assets& operator=(Assets&&) = delete;

    void collect_garbage() {
        std::vector<std::string> keys;

        for (const auto& [key, value] : *this) {
            if (value.use_count() == 1) {
                keys.push_back(key);
            }
        }

        for (const auto& key : keys) {
            release(key);
        }
    }


    template<typename T>
    std::shared_ptr<T> get(const std::string& filepath) {
        if (!fs::exists(filepath)) {
            return nullptr;
        }

        {
            std::scoped_lock lk(loadMutex);

            // if it already has an asset pointer it means some other thread already added it,
            // so just return whats already there, the thread that added it is responsible for loading.
            if (find(filepath) != end()) {
                return std::static_pointer_cast<T>(operator[](filepath));
            } else {
                operator[](filepath) = std::shared_ptr<Asset>(new T(filepath));
            }

            // only get here if this thread created the asset pointer, try to load it.
            // if load success we return the asset pointer
            if (operator[](filepath)->load(filepath)) {
                return std::static_pointer_cast<T>(operator[](filepath));
            } else {
                // if load failed, lock -> remove asset pointer -> return nullptr
                erase(filepath);
                return nullptr;
            }
        }
    }

    void release(const std::string& filepath) {
        std::scoped_lock(releaseMutex);
        
        if (find(filepath) != end()) {
            erase(filepath);
        }
    }

private:
    std::mutex loadMutex;
    std::mutex releaseMutex;
};



class TextureAsset : public Asset {
public:
    using Ptr = std::shared_ptr<TextureAsset>;

    TextureAsset() = default;
    TextureAsset(const std::string& filepath);

    static std::string convert(const std::string& filepath);
    virtual bool load(const std::string& filepath) override;

    template<class Archive> 
    void serialize(Archive& archive) {
        archive(data);
    }

    const DDS_HEADER* header();
    char* const getData();
    uint32_t getDataSize() const;

private:
    std::vector<char> data;
};



class ScriptAsset : public Asset {
public:
    using Ptr = std::shared_ptr<ScriptAsset>;

    ScriptAsset() = default;
    ScriptAsset(const std::string& filepath);
    virtual ~ScriptAsset();

    static std::string convert(const std::string& filepath);
    virtual bool load(const std::string& filepath) override;

    void enumerateSymbols();

    template<class Archive> 
    void serialize(Archive& archive) {
    }

    HMODULE getModule() { return hmodule; }

private:
    HMODULE hmodule;
};


} // raekor

CEREAL_REGISTER_TYPE(Raekor::ScriptAsset);
CEREAL_REGISTER_TYPE(Raekor::TextureAsset);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Raekor::Asset, Raekor::ScriptAsset);
CEREAL_REGISTER_POLYMORPHIC_RELATION(Raekor::Asset, Raekor::TextureAsset);
