#pragma once

#include "pch.h"

namespace Raekor {

class Texture {

public:
    static Texture * construct(const std::string& path);
    virtual void bind() const = 0;
    virtual void unbind() const = 0;
    virtual unsigned int get_id() const = 0;
    std::string get_path() const { return filepath; }

protected:
    std::string filepath;
    unsigned int id;
};

class GLTexture : public Texture {

public:
    GLTexture(const std::string& path);
    ~GLTexture();
    virtual void bind() const override;
    virtual void unbind() const override;
    inline virtual unsigned int get_id() const override { return id; }
};

} //Namespace Raekor