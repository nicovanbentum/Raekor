#pragma once

#include "widget.h"

namespace Raekor {

class ConsoleWidget : public IWidget {
public:
    ConsoleWidget(Editor* editor);
    virtual void draw(float dt) override;
    virtual void onEvent(const SDL_Event& ev) override {}

private:
    static int editCallback(ImGuiInputTextCallbackData* data);

public:
    int activeItem = 0;
    bool ScrollToBottom = false;
    std::string inputBuffer;
    std::vector<std::string> items;
};

} // raekor