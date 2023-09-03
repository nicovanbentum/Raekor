#pragma once

#include "rtti.h"

namespace Raekor {

class Editor;
class Scene;
class Assets;
class Physics;
class Widgets;
class IRenderInterface;
class Application;
using Entity = uint32_t;

class IWidget {
    RTTI_DECLARE_TYPE(IWidget);

public:
    IWidget(Application* inApp, const std::string& inTitle);
    virtual void Draw(Widgets* inWidgets, float inDeltaTime) = 0;
    virtual void OnEvent(Widgets* inWidgets, const SDL_Event& inEvent) = 0;

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
    IRenderInterface& GetRenderInterface();

    Entity GetActiveEntity();
    void   SetActiveEntity(Entity inEntity);

    Application* m_Editor;
    std::string m_Title;
    bool m_Open = true;
    bool m_Visible = false;
    bool m_Focused = false;
};


class Widgets {
public:

    template<typename T>
    void Register(Application* inApp) {
        static_assert(std::is_base_of<IWidget, T>());
        m_Widgets.emplace_back(std::make_shared<T>(inApp));
    }

    template<typename T>
    std::shared_ptr<T> GetWidget() {
        for (const auto& widget : m_Widgets)
            if (widget->GetRTTI() == gGetRTTI<T>())
                return std::static_pointer_cast<T>(widget);

        return nullptr;
    }

    void Draw(float inDeltaTime);
    void OnEvent(const SDL_Event& inEvent);

    auto begin() { return m_Widgets.begin(); }
    auto end()   { return m_Widgets.end(); }

private:
    std::vector<std::shared_ptr<IWidget>> m_Widgets;
};

}
