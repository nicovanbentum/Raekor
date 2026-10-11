#include "PCH.h"
#include "GameConsole.h"
#include "CVars.h"
#include "Iter.h"

namespace RK {

static bool sContainsNoCase(StringView inText, StringView inQuery)
{
    const auto match = std::ranges::search(inText, inQuery, [](char inA, char inB) { return std::tolower(uint8_t(inA)) == std::tolower(uint8_t(inB)); });
    return !match.empty() || inQuery.empty();
}


GameConsole::GameConsole()
{
    m_LogSink = g_Logger.AddSink([this](const LogMessage& inMessage)
    {
        std::scoped_lock lock(m_ItemsMutex);

        if (m_Items.size() >= cMaxItems)
            m_Items.erase(m_Items.begin(), m_Items.begin() + cMaxItems / 4);

        m_Items.push_back(inMessage);
    }, true);
}


GameConsole::~GameConsole()
{
    g_Logger.RemoveSink(m_LogSink);
}


void GameConsole::Toggle()
{
    m_Open = !m_Open;

    if (m_Open)
    {
        m_FocusInput = true;
        m_ScrollToBottom = true;
    }
}


void GameConsole::Draw(float inDeltaTime)
{
    const float target = m_Open ? 1.0f : 0.0f;
    const float step = inDeltaTime / cSlideDuration;
    m_Openness = m_Openness < target ? glm::min(m_Openness + step, target) : glm::max(m_Openness - step, target);

    if (m_Openness <= 0.0f)
        return;

    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    const float height = glm::round(display_size.y * cHeightFraction);
    const float slide = 1.0f - ( 1.0f - m_Openness ) * ( 1.0f - m_Openness );

    ImGui::SetNextWindowPos(ImVec2(0.0f, glm::round(-height * ( 1.0f - slide ))));
    ImGui::SetNextWindowSize(ImVec2(display_size.x, height));
    ImGui::SetNextWindowBgAlpha(0.92f);

    if (m_FocusInput)
        ImGui::SetNextWindowFocus();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("##GameConsole", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    ImGui::PopStyleVar(2);

    constexpr StaticArray level_colors =
    {
        ImVec4(0.55f, 0.55f, 0.55f, 1.0f),
        ImVec4(0.90f, 0.90f, 0.90f, 1.0f),
        ImVec4(1.00f, 0.80f, 0.30f, 1.0f),
        ImVec4(1.00f, 0.40f, 0.40f, 1.0f)
    };

    const float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();

    ImGui::BeginChild("##GameConsoleLog", ImVec2(0.0f, -footer_height), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 1.0f));

    {
        std::scoped_lock lock(m_ItemsMutex);

        ImGuiListClipper clipper;
        clipper.Begin(int(m_Items.size()));

        while (clipper.Step())
        {
            for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; index++)
            {
                const LogMessage& item = m_Items[index];

                ImGui::PushStyleColor(ImGuiCol_Text, level_colors[item.mLevel]);
                ImGui::Text("[%s] %s", item.mCategory.c_str(), item.mText.c_str());
                ImGui::PopStyleColor();
            }
        }
    }

    if (m_ScrollToBottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
        m_ScrollToBottom = false;
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImGui::Separator();

    if (m_HistoryIndex != -1 && m_History[m_HistoryIndex] != m_InputBuffer)
        m_HistoryIndex = -1;

    UpdateSuggestions();

    if (m_Open)
    {
        if (m_FocusInput || !ImGui::IsAnyItemActive())
        {
            ImGui::SetKeyboardFocusHere();
            m_FocusInput = false;
        }

        const ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;

        ImGui::SetNextItemWidth(-FLT_MIN);

        if (ImGui::InputTextWithHint("##GameConsoleInput", "Enter a console variable and value, Tab completes, Up/Down selects", &m_InputBuffer, input_flags, sInputCallback, this))
        {
            if (!m_InputBuffer.empty())
                ExecuteCommand(m_InputBuffer);

            m_InputBuffer.clear();
            m_FocusInput = true;
        }
    }
    else
    {
        ImGui::Dummy(ImVec2(0.0f, ImGui::GetFrameHeight()));
    }

    const ImVec2 suggestions_pos = ImVec2(ImGui::GetItemRectMin().x, ImGui::GetWindowPos().y + ImGui::GetWindowHeight());

    ImGui::End();

    if (m_Open && !m_Suggestions.empty())
        DrawSuggestions(suggestions_pos);
}


void GameConsole::DrawSuggestions(ImVec2 inPos)
{
    float name_width = 0.0f;

    for (const String& suggestion : m_Suggestions)
        name_width = glm::max(name_width, ImGui::CalcTextSize(suggestion.c_str()).x);

    ImGui::SetNextWindowPos(inPos);
    ImGui::BeginTooltip();

    for (const auto& [index, suggestion] : gEnumerate(m_Suggestions))
    {
        const bool is_selected = int(index) == m_SuggestionIndex;

        ImGui::PushStyleColor(ImGuiCol_Text, is_selected ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::Selectable(suggestion.c_str(), is_selected, ImGuiSelectableFlags_None, ImVec2(name_width, 0.0f));
        ImGui::PopStyleColor();

        ImGui::SameLine(name_width + ImGui::GetStyle().ItemSpacing.x * 4.0f);
        ImGui::TextUnformatted(g_CVariables->GetValue(suggestion).c_str());
    }

    ImGui::EndTooltip();
}


void GameConsole::ExecuteCommand(const String& inCommand)
{
    if (m_History.empty() || m_History.back() != inCommand)
        m_History.push_back(inCommand);

    m_HistoryIndex = -1;
    m_ScrollToBottom = true;

    g_CVariables->ExecuteCommand(inCommand);
}


void GameConsole::UpdateSuggestions()
{
    const size_t space = m_InputBuffer.find(' ');
    const String query = m_InputBuffer.substr(0, space);

    if (query != m_SuggestionQuery)
    {
        m_SuggestionQuery = query;
        m_SuggestionIndex = 0;
    }

    m_Suggestions.clear();

    if (query.empty())
        return;

    for (const auto& [name, cvar] : g_CVariables->GetCVars())
    {
        const bool matches = space == String::npos ? sContainsNoCase(name, query) : name == query;

        if (matches)
            m_Suggestions.push_back(name);
    }

    std::ranges::sort(m_Suggestions, [&query](const String& inA, const String& inB)
    {
        const bool a_prefix = inA.starts_with(query);
        const bool b_prefix = inB.starts_with(query);
        return a_prefix != b_prefix ? a_prefix : inA < inB;
    });

    if (m_Suggestions.size() > cMaxSuggestions)
        m_Suggestions.resize(cMaxSuggestions);

    m_SuggestionIndex = glm::clamp(m_SuggestionIndex, 0, glm::max(int(m_Suggestions.size()) - 1, 0));
}


int GameConsole::sInputCallback(ImGuiInputTextCallbackData* inData)
{
    GameConsole* console = static_cast<GameConsole*>(inData->UserData);

    if (inData->EventFlag == ImGuiInputTextFlags_CallbackCompletion && !console->m_Suggestions.empty())
    {
        const String completion = console->m_Suggestions[console->m_SuggestionIndex] + ' ';

        inData->DeleteChars(0, inData->BufTextLen);
        inData->InsertChars(0, completion.c_str());
    }

    if (inData->EventFlag == ImGuiInputTextFlags_CallbackHistory)
    {
        const bool is_up = inData->EventKey == ImGuiKey_UpArrow;

        if (console->m_HistoryIndex == -1 && inData->BufTextLen > 0 && !console->m_Suggestions.empty())
        {
            const int count = int(console->m_Suggestions.size());
            console->m_SuggestionIndex = ( console->m_SuggestionIndex + ( is_up ? count - 1 : 1 ) ) % count;
            return 0;
        }

        if (console->m_History.empty())
            return 0;

        const int count = int(console->m_History.size());

        if (is_up)
            console->m_HistoryIndex = console->m_HistoryIndex == -1 ? count - 1 : glm::max(console->m_HistoryIndex - 1, 0);
        else if (console->m_HistoryIndex != -1)
            console->m_HistoryIndex = console->m_HistoryIndex + 1 < count ? console->m_HistoryIndex + 1 : -1;
        else
            return 0;

        inData->DeleteChars(0, inData->BufTextLen);

        if (console->m_HistoryIndex != -1)
            inData->InsertChars(0, console->m_History[console->m_HistoryIndex].c_str());
    }

    return 0;
}

} // RK
