#pragma once

#include "widget.h"

namespace Raekor {

class ConsoleWidget : public IWidget {
public:
    RTTI_CLASS_HEADER(ConsoleWidget);

    ConsoleWidget(Application* inApp);
    virtual void Draw(Widgets* inWidgets, float inDeltaTime) override;
    virtual void OnEvent(Widgets* inWidgets, const SDL_Event& inEvent) override {}

    void LogMessage(const std::string& inMessage);

private:
    static int sEditCallback(ImGuiInputTextCallbackData* data);

public:
    int m_ActiveItem = 0;
    bool m_ShouldScrollToBottom = false;

    std::string m_InputBuffer = "";
    std::mutex  m_ItemsMutex;
    std::vector<std::string> m_Items;
};

} // raekor