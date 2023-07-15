#pragma once

#include "Raekor/async.h"
#include "Raekor/scene.h"
#include "Raekor/assets.h"
#include "Raekor/application.h"

namespace Raekor {

class Editor;
class Scene;
class Assets;
class Physics;
class GLRenderer;

class IWidget {
    RTTI_CLASS_HEADER(IWidget);

public:
    IWidget(Application* inApp, const std::string& inTitle);
    virtual void Draw(float inDeltaTime) = 0;
    virtual void OnEvent(const SDL_Event& inEvent) = 0;

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
    IRenderer& GetRenderer();

    Entity GetActiveEntity();
    void   SetActiveEntity(Entity inEntity);

    Application* m_Editor;
    std::string m_Title;
    bool m_Open = true;
    bool m_Visible = false;
    bool m_Focused = false;
};

}
