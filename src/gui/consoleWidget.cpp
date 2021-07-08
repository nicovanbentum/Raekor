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

    for (const auto& cvar : ConVars::get()) {
        items.push_back(cvar.first.c_str());
    }
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

    auto callback = [](ImGuiInputTextCallbackData* data) -> int {
        ConsoleWidget* console = (ConsoleWidget*)data->UserData;

        if (data->EventKey == ImGuiKey_Tab && data->BufTextLen) {
            ImGuiTextFilter filter(data->Buf);

            int index = 0;

            for (const auto& cvar : ConVars::get()) {
                if (filter.PassFilter(cvar.first.c_str())) {
                    if (index == console->activeItem) {
                        data->DeleteChars(0, data->BufTextLen);
                        data->InsertChars(0, std::string(cvar.first + ' ').c_str());
                        break;
                    }
                    index++;
                }
            }
        }

        if (data->EventKey == ImGuiKey_DownArrow) {
            console->activeItem++;
        }

        if (data->EventKey == ImGuiKey_UpArrow) {
            if (console->activeItem) {
                console->activeItem--;
            }
        }

        return 0;
    };

    ImGui::PushItemWidth(ImGui::GetWindowWidth());

    auto flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;

    if (ImGui::InputText("##Input", InputBuf, IM_ARRAYSIZE(InputBuf), flags, callback, (void*)this)) {
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
    }

    if (strlen(InputBuf) > 0) {
        ImGuiTextFilter filter(InputBuf);

        ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y));

        int count = 0;
        for (const auto& mapping : ConVars::get()) {
            if (filter.PassFilter(mapping.first.c_str())) {
                count++;
            }
        }

        ImGui::SetNextWindowSize(ImVec2(200, (count + 1) * ImGui::GetTextLineHeightWithSpacing()));
        ImGui::BeginTooltip();

        count = 0;
        for (const auto& mapping : ConVars::get()) {
            if (filter.PassFilter(mapping.first.c_str())) {
                std::string cvarText = mapping.first + " " + ConVars::get(mapping.first) + '\n';

                if (count == activeItem) {
                    ImGui::Selectable(cvarText.c_str(), true);
                } else {
                    ImGui::TextUnformatted(cvarText.c_str());
                }

                count++;
            }
        }

        activeItem = activeItem > count ? count : activeItem;

        ImGui::EndTooltip();
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

    ImGui::End();
}

void ConsoleWidget::ExecCommand(const char* command_line) {
    AddLog(command_line);

    // On command input, we scroll to bottom even if AutoScroll==false
    ScrollToBottom = true;
}

}