#pragma once

namespace Raekor {

class Editor;

class IWidget {
public:
    IWidget(Editor* editor);
    virtual void draw() = 0;

    void show() { visible = true; }
    void hide() { visible = false; }
    bool isVisible() { return visible; }

protected:
    bool visible = true;
    ImDrawList* drawList;
    Editor* editor;
};

}
