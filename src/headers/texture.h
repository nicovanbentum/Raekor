#pragma once

#include "pch.h"

namespace Raekor {

class Texture {

public:
    static Texture* construct(const std::string& path);
    static Texture* construct(const std::vector<std::string>& face_files);
    virtual void bind() const = 0;
    std::string get_path() const { return filepath; }

protected:
    std::string filepath;
};

class GLTexture : public Texture {

public:
    GLTexture(const std::string& path);
    ~GLTexture();
    virtual void bind() const override;

private:
    unsigned int id;
};

class GLTextureCube : public Texture {

public:
    GLTextureCube(const std::vector<std::string>& face_files);
    ~GLTextureCube();
    virtual void bind() const override;

private:
    unsigned int id;
};

} //Namespace Raekor