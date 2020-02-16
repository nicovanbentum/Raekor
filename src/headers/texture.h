#pragma once

#include "pch.h"

namespace Raekor {

class Texture {
public:
    virtual ~Texture() {}
    static Texture* construct(const std::string& path);
    static Texture* construct(const std::array<std::string, 6>& face_files);
    static Texture* construct(const Stb::Image& image);
    virtual void bind(uint32_t slot) const = 0;
    std::string get_path() const { return filepath; }

protected:
    std::string filepath;

public:
    bool hasAlpha = false;
};

class GLTexture : public Texture {

public:
    friend class GLShader;
    GLTexture(const std::string& path);
    GLTexture(const Stb::Image& image);
    ~GLTexture();
    virtual void bind(uint32_t slot) const override;

private:
    unsigned int id;
};

class GLTextureCube : public Texture {

public:
    GLTextureCube(const std::array<std::string, 6>& face_files);
    ~GLTextureCube();
    virtual void bind(uint32_t slot) const override;

private:
    unsigned int id;
};

} //Namespace Raekor