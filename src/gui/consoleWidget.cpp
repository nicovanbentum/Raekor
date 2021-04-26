#include "pch.h"
#include "consoleWidget.h"
#include "editor.h"

namespace Raekor {



ConsoleWidget::ConsoleWidget(Editor* editor) : IWidget(editor, "Console") {
    ClearLog();
    memset(InputBuf, 0, sizeof(InputBuf));
    HistoryPos = -1;
    AutoScroll = true;
    ScrollToBottom = false;
}

ConsoleWidget::~ConsoleWidget() {
    ClearLog();
    for (int i = 0; i < History.Size; i++)
        free(History[i]);
}

char* ConsoleWidget::Strdup(const char* str) { size_t len = strlen(str) + 1; void* buf = malloc(len); IM_ASSERT(buf); return (char*)memcpy(buf, (const void*)str, len); }
void  ConsoleWidget::Strtrim(char* str) { char* str_end = str + strlen(str); while (str_end > str && str_end[-1] == ' ') str_end--; *str_end = 0; }

void ConsoleWidget::ClearLog() {
    for (int i = 0; i < Items.Size; i++)
        free(Items[i]);
    Items.clear();
}

void ConsoleWidget::draw() {
    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(title.c_str(), &visible)) {
        ImGui::End();
        return;
    }

    // Command-line
    bool reclaim_focus = false;

    ImGui::PushItemWidth(ImGui::GetWindowWidth());
    if (ImGui::InputText("##Input", InputBuf, IM_ARRAYSIZE(InputBuf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackAlways, &TextEditCallbackStub, (void*)this)) {
        char* s = InputBuf;
        Strtrim(s);
        if (s[0]) {
            ExecCommand(s);
            std::string expression = std::string(s);
            std::istringstream stream(expression);
            std::string name, value;
            stream >> name >> value;

            bool success = ConVars::set(name, value);
            if (!success) {
                ExecCommand(std::string("Failed to set var " + name + " to " + value).c_str());
            }
        }
        strcpy(s, "");
        reclaim_focus = true;
    }

    ImGui::PopItemWidth();

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Close Console"))
            visible = false;
        ImGui::EndPopup();
    }

    ImGui::Separator();

    const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);
    if (ImGui::BeginPopupContextWindow()) {
        if (ImGui::Selectable("Clear")) ClearLog();
        ImGui::EndPopup();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
    for (int i = 0; i < Items.Size; i++) {
        const char* item = Items[i];
        if (!Filter.PassFilter(item))
            continue;

        ImGui::TextUnformatted(item);
    }

    if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
        ImGui::SetScrollHereY(1.0f);
    ScrollToBottom = false;

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::Separator();

    // Auto-focus on window apparition
    ImGui::SetItemDefaultFocus();
    if (reclaim_focus)
        ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

    ImGui::End();
}

void    ConsoleWidget::ExecCommand(const char* command_line) {
    AddLog(command_line);

    // On command input, we scroll to bottom even if AutoScroll==false
    ScrollToBottom = true;
}

int ConsoleWidget::TextEditCallbackStub(ImGuiInputTextCallbackData* data) // In C++11 you are better off using lambdas for this sort of forwarding callbacks
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways && data->BufTextLen) {

        if (data->EventKey == ImGuiKey_Tab) {
            std::cout << "completed" << std::endl;
        }

        ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y));
        ImGuiTextFilter filter(data->Buf);

        int count = 0;
        for (const auto& mapping : ConVars::getIterable()) {
            if (filter.PassFilter(mapping.first.c_str())) {
                count++;
            }
        }

        ImGui::SetNextWindowSize(ImVec2(200, (count + 1) * ImGui::GetTextLineHeightWithSpacing()));
        ImGui::BeginTooltip();

        for (const auto& mapping : ConVars::getIterable()) {
            if (filter.PassFilter(mapping.first.c_str())) {
                std::string cvarText = mapping.first + " " + ConVars::get(mapping.first) + '\n';
                ImGui::TextUnformatted(cvarText.c_str());
            }
        }

        ImGui::EndTooltip();
    }

    ConsoleWidget* console = (ConsoleWidget*)data->UserData;
    return console->TextEditCallback(data);
}

int ConsoleWidget::TextEditCallback(ImGuiInputTextCallbackData* data) {
    return 0;
}

}