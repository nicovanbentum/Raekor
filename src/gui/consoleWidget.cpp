#include "pch.h"
#include "consoleWidget.h"
#include "editor.h"

namespace Raekor {

ConsoleWidget::ConsoleWidget(Editor* editor) : IWidget(editor, "Console") {}



void ConsoleWidget::draw(float dt) {
    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(title.c_str(), &visible, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        ImGui::End();
        return;
    }

    const float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();

    ImGui::BeginChild("##LOG", ImVec2(ImGui::GetContentRegionAvail().x, -footerHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

    if (ImGui::BeginPopupContextWindow()) {
        if (ImGui::Selectable("clear")) items.clear();
        ImGui::EndPopup();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
    for (const auto& item : items) {
        ImGui::TextUnformatted(item.c_str());
    }

    if (ScrollToBottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
        ScrollToBottom = false;
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImVec2 cursorAfterLog = ImGui::GetCursorScreenPos();

    ImGui::Separator();

    ImGui::SetItemDefaultFocus();

    ImGui::PushItemWidth(ImGui::GetWindowWidth());
    auto flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;

    if (ImGui::InputText("##Input", &inputBuffer, flags, editCallback, (void*)this)) {
        if (!inputBuffer.empty()) {
            items.push_back(inputBuffer);
            ScrollToBottom = true;

            std::istringstream stream(inputBuffer);
            std::string name, value;
            stream >> name >> value;

            bool success = ConVars::set(name, value);
            if (!success) {
                if (ConVars::get(name).empty()) {
                    items.emplace_back("cvar \"" + name + "\" does not exist.");
                } else if(value.empty()) {
                    items.emplace_back("Please provide a value.");
                } else {
                    items.emplace_back("Failed to set cvar " + name + " to " + "\"" + value + "\"");
                }
            }
        }

        inputBuffer.clear();
        ImGui::SetKeyboardFocusHere();
    }

    if (!inputBuffer.empty()) {
        ImGuiTextFilter filter(inputBuffer.c_str());

        const int suggestionCount = static_cast<int>(ImGui::GetWindowHeight() * (2.0f / 3.0f) / ImGui::GetTextLineHeightWithSpacing());
        const float suggestionWidth = ImGui::GetItemRectSize().x - ImGui::GetStyle().FramePadding.x * 2;
        const float suggestionHeight = ImGui::GetTextLineHeightWithSpacing() * suggestionCount;

        ImGui::SetNextWindowSize(ImVec2(suggestionWidth, suggestionHeight));
        ImGui::SetNextWindowPos(ImVec2(cursorAfterLog.x, cursorAfterLog.y - suggestionHeight));
        ImGui::BeginTooltip();

        int count = 0;
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
    ImGui::Separator();
    ImGui::End();
}



int ConsoleWidget::editCallback(ImGuiInputTextCallbackData* data) {
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
}

}