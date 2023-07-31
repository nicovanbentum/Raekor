#pragma once

#include "Raekor/application.h"
#include "Raekor/physics.h"
#include "Raekor/editor.h"
#include "Raekor/assets.h"
#include "Raekor/scene.h"
#include "Raekor/gui.h"
#include "renderer.h"
#include "widget.h"

namespace Raekor::GL {

class GLApp : public IEditor {
public:
    GLApp();
    virtual ~GLApp() = default;

    void OnUpdate(float dt) override;
    void OnEvent(const SDL_Event& event) override;

    IRenderer* GetRenderer() { return &m_Renderer; }

private:
    Renderer m_Renderer;
};


} // Raekor