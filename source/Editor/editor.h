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
    Assets m_Assets;
    Physics m_Physics;
    GLRenderer m_Renderer;
    Entity m_ActiveEntity = sInvalidEntity;
    std::vector<std::shared_ptr<IWidget>> m_Widgets;
};


template<typename T>
std::shared_ptr<T> Editor::GetWidget() {
    for (const auto& widget : m_Widgets)
        if (widget->GetRTTI() == gGetRTTI<T>())
            return std::static_pointer_cast<T>(widget);

    return nullptr;
}


class MyThing {
    RTTI_CLASS_HEADER(MyThing);

public:
    int m_MyInt = 0;
    float m_MyFloat = 0.0f;
    bool m_MyBool = false;
    std::string m_MyString = "";
    glm::vec4 m_Vec = glm::vec4(0.0f);
    std::vector<int> m_Vector;
};


class MyPtrThing {
    RTTI_CLASS_HEADER(MyPtrThing);

public:
    MyThing* m_Ptr;
};


} // raekor