#pragma once

#include "application.h"

namespace Raekor {

class CompilerApp : public Application {
public:
    CompilerApp();
    ~CompilerApp();

    virtual void OnUpdate(float inDeltaTime) override;
    virtual void OnEvent(const SDL_Event& inEvent) override;

private:
    bool m_WasClosed = false;
    int m_ResizeCounter = 0;
    SDL_Renderer* m_Renderer;
    std::set<Path> m_Files;
};

}