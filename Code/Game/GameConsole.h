#pragma once

namespace RK {

class GameConsole
{
public:
    static constexpr size_t cMaxItems = 2048;
    static constexpr size_t cMaxSuggestions = 10;
    static constexpr float cSlideDuration = 0.15f;
    static constexpr float cHeightFraction = 0.4f;

    GameConsole();
    ~GameConsole();

    bool IsOpen() const { return m_Open; }

    void Toggle();
    void Close() { m_Open = false; }

    void Draw(float inDeltaTime);

private:
    void ExecuteCommand(const String& inCommand);
    void UpdateSuggestions();
    void DrawSuggestions(ImVec2 inPos);

    static int sInputCallback(ImGuiInputTextCallbackData* inData);

    bool m_Open = false;
    bool m_FocusInput = false;
    bool m_ScrollToBottom = false;
    float m_Openness = 0.0f;

    String m_InputBuffer;
    Array<String> m_History;
    int m_HistoryIndex = -1;

    String m_SuggestionQuery;
    Array<String> m_Suggestions;
    int m_SuggestionIndex = 0;

    uint32_t m_LogSink = 0;
    std::mutex m_ItemsMutex;
    Array<LogMessage> m_Items;
};

} // RK
