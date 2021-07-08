#pragma once

#include "widget.h"

namespace Raekor {

    // TODO: clean this thing up
class ConsoleWidget : public IWidget {
public:
    ConsoleWidget(Editor* editor);
    ~ConsoleWidget();
    virtual void draw() override;

    static char* Strdup(const char* str);
    static void Strtrim(char* str);

    void ClearLog();

    void AddLog(const char* fmt, ...) IM_FMTARGS(2) {
        // FIXME-OPT
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
        buf[IM_ARRAYSIZE(buf) - 1] = 0;
        va_end(args);
        Items.insert(Items.begin(), Strdup(buf));
    }

    void    ExecCommand(const char* command_line);

public:
    char                  InputBuf[256];
    ImVector<char*>       Items;
    ImVector<char*>       History;
    int                   HistoryPos;
    ImGuiTextFilter       Filter;
    bool                  AutoScroll;
    bool                  ScrollToBottom;

    int                   activeItem = 0;
    std::vector<const char*> items;
};

} // raekor