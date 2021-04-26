#pragma once

namespace Raekor {

class Editor;

class IWidget {
public:
    IWidget(Editor* editor, const std::string& title);
    virtual void draw() = 0;

    void show() { visible = true; }
    void hide() { visible = false; }
    bool& isVisible() { return visible; }
    void setVisible(bool value) { visible = value; }
    const std::string& getTitle() { return title; }

protected:
    Editor* editor;
    std::string title;
    bool visible = true;
};

}
