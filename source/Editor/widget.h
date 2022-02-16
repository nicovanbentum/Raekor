#pragma once

#include "Raekor/async.h"
#include "Raekor/scene.h"
#include "Raekor/assets.h"
#include "renderer.h"

namespace Raekor {

class Editor;
class Scene;
class Assets;
class GLRenderer;

#define TYPE_ID(Type) \
constexpr static uint32_t m_TypeID = Hash32Bit(#Type); \
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

    void show() { visible = true; }
    void hide() { visible = false; }
    bool& isVisible() { return visible; }
    void setVisible(bool value) { visible = value; }
    const std::string& getTitle() { return title; }
    bool isFocused() { return focused; }

    Scene& scene();
    Assets& assets();
    GLRenderer& renderer();


protected:
    Editor* editor;
    std::string title;
    bool visible = true;
    bool focused = false;
};

}
