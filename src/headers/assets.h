#pragma once

namespace Raekor {

class TextureAsset {
public:
    ~TextureAsset();
    
    static bool create(const std::string& filepath);
    bool load(const std::string& filepath);

private:
    char* data;
};

} // raekor