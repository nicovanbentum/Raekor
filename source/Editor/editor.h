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

    void onUpdate(float dt) override;
    void onEvent(const SDL_Event& event) override;

    template<typename T>
    std::shared_ptr<T> GetWidget() {
        for (const auto& widget : widgets) {
            if (widget->GetTypeID() == T::m_TypeID) {
                return std::static_pointer_cast<T>(widget);
            }
        }
        
        return nullptr;
    }

    const std::vector<std::shared_ptr<IWidget>>& GetWidgets() { return widgets; }

    entt::entity active = entt::null;

private:
    Scene scene;
    Assets assets;
    Physics physics;
    GLRenderer renderer;
    std::vector<std::shared_ptr<IWidget>> widgets;
};

} // raekor