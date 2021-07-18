#pragma once

#include "async.h"
#include "scene.h"
#include "assets.h"
#include "renderer.h"

namespace Raekor {

class Editor;
class Scene;
class Assets;
class GLRenderer;

class IWidget {
public:
    IWidget(Editor* editor, const std::string& title);
    virtual void draw() = 0;

    void show() { visible = true; }
    void hide() { visible = false; }
    bool& isVisible() { return visible; }
    void setVisible(bool value) { visible = value; }
    const std::string& getTitle() { return title; }

    Async& async();
    Scene& scene();
    Assets& assets();
    GLRenderer& renderer();


protected:
    Editor* editor;
    std::string title;
    bool visible = true;
};

}
