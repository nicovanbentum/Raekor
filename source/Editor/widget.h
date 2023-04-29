#pragma once

#include "Raekor/async.h"
#include "Raekor/scene.h"
#include "Raekor/assets.h"
#include "renderer.h"

namespace Raekor {

class Editor;
class Scene;
class Assets;
class Physics;
class GLRenderer;

class IWidget {
    RTTI_CLASS_HEADER(IWidget);

public:
    IWidget(Editor* editor, const std::string& inTitle);
    virtual void draw(float dt) = 0;
    virtual void onEvent(const SDL_Event& ev) = 0;

    void Show() { m_Open = true; }
    void Hide() { m_Open = false; }
    
    bool IsOpen()    { return m_Open; }
    bool IsVisible() { return m_Visible; }
    bool IsFocused() { return m_Focused; }

    const std::string& GetTitle() { return m_Title; }

protected:
    // Need to be defined in the cpp to avoid circular dependencies
    Scene& GetScene();
    Assets& GetAssets();
    Physics& GetPhysics();
    Entity& GetActiveEntity();
    GLRenderer& GetRenderer();

    Editor* m_Editor;
    std::string m_Title;
    bool m_Open = true;
    bool m_Visible = false;
    bool m_Focused = false;
};

}
