#pragma once

#include "Raekor/application.h"
#include "Raekor/physics.h"
#include "Raekor/assets.h"
#include "Raekor/scene.h"
#include "Raekor/gui.h"
#include "widget.h"

namespace Raekor {

class Editor : public Application {
public:
    friend class IWidget;

    Editor();
    virtual ~Editor() = default;

    void OnUpdate(float dt) override;
    void OnEvent(const SDL_Event& event) override;

    template<typename T>
    std::shared_ptr<T>  GetWidget();
    const auto& GetWidgets() { return m_Widgets; }

private:
    Scene m_Scene;
    Assets m_Assets;
    Physics m_Physics;
    GLRenderer m_Renderer;
    Entity m_ActiveEntity = sInvalidEntity;
    std::vector<std::shared_ptr<IWidget>> m_Widgets;
};


template<typename T>
std::shared_ptr<T> Editor::GetWidget() {
    for (const auto& widget : m_Widgets) {
        if (widget->GetTypeID() == T::m_TypeID) {
            return std::static_pointer_cast<T>(widget);
        }
    }

    return nullptr;
}

} // raekor