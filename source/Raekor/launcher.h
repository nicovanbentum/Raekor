#pragma once

#include "application.h"

namespace Raekor {

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
    SDL_Renderer* m_Renderer;
};

}