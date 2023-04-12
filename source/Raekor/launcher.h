#pragma once

#include "application.h"

namespace Raekor {


class SDL_Image {
public:
    ~SDL_Image();
    
    bool Load(SDL_Renderer* inRenderer, const Path& inPath);
    bool IsLoaded() const { return m_PixelData != nullptr; }

    uint8_t*     GetPixels()  const { return m_PixelData; }
    SDL_Surface* GetSurface() const { return m_Surface;   }
    SDL_Texture* GetTexture() const { return m_Texture;   }

private:
    uint8_t*     m_PixelData = nullptr;
    SDL_Surface* m_Surface   = nullptr;
    SDL_Texture* m_Texture   = nullptr;
};



class Launcher : public Application {
public:
    Launcher();
    ~Launcher();
    
    virtual void OnUpdate(float inDeltaTime) override;
    virtual void OnEvent(const SDL_Event& inEvent) override;

    bool ShouldLaunch() const { return !m_WasClosed; }

private:
    int m_ResizeCounter = 0;
    bool m_WasClosed = false;
    SDL_Image m_BgImage;
    SDL_Renderer* m_Renderer;
};

}