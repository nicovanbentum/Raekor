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

#define TYPE_ID(Type) \
constexpr static uint32_t m_TypeID = gHash32Bit(#Type); \
virtual const uint32_t GetTypeID() const override { return m_TypeID; }

class ITypeID {
public:
    virtual const uint32_t GetTypeID() const = 0;
};

class IWidget : public ITypeID {
public:
    IWidget(Editor* editor, const std::string& title);
    virtual void draw(float dt) = 0;
    virtual void onEvent(const SDL_Event& ev) = 0;

    void Show() { visible = true; }
    void Hide() { visible = false; }
    
    bool IsVisible() { return visible; }
    bool isFocused() { return focused; }

    const std::string& GetTitle() { return title; }

protected:
    // Need to be defined in the cpp to avoid circular dependencies
    Scene& GetScene();
    Assets& GetAssets();
    Physics& GetPhysics();
    GLRenderer& GetRenderer();
    entt::entity& GetActiveEntity();

    Editor* editor;
    std::string title;
    bool visible = true;
    bool focused = false;
};

}
