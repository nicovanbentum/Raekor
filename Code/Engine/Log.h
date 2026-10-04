#pragma once

namespace RK {

enum ELogLevel
{
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_COUNT
};

const char* gToString(ELogLevel inLevel);


struct LogMessage
{
    ELogLevel mLevel = LOG_LEVEL_INFO;
    String mCategory;
    String mText;
    double mTime = 0.0;
    uint32_t mThreadID = 0;
};

String gSerializeLogMessage(const LogMessage& inMessage);
bool gDeserializeLogMessage(StringView inData, LogMessage& outMessage);


class Logger
{
public:
    using Sink = std::function<void(const LogMessage&)>;

    static constexpr size_t cMaxHistorySize = 4096;

    Logger();
    ~Logger();

    void Log(ELogLevel inLevel, StringView inCategory, StringView inText);

    uint32_t AddSink(const Sink& inSink, bool inReplayHistory = false);
    void RemoveSink(uint32_t inSinkID);

    bool OpenFile(const Path& inPath);
    void CloseFile();

    void SetMinimumLevel(ELogLevel inLevel) { m_MinimumLevel = inLevel; }
    ELogLevel GetMinimumLevel() const { return m_MinimumLevel; }

    Array<LogMessage> GetHistory() const;
    void ClearHistory();

private:
    void WriteToConsole(const LogMessage& inMessage);
    void WriteToFile(const LogMessage& inMessage);

    mutable Mutex m_Mutex;
    Atomic<ELogLevel> m_MinimumLevel = IF_DEBUG_ELSE(LOG_LEVEL_DEBUG, LOG_LEVEL_INFO);
    std::chrono::steady_clock::time_point m_StartTime;

    std::ofstream m_File;
    uint32_t m_NextSinkID = 1;
    Array<Pair<uint32_t, Sink>> m_Sinks;

    size_t m_HistoryStart = 0;
    Array<LogMessage> m_History;
};


extern Logger g_Logger;


template<typename ...Args>
inline void gLog(ELogLevel inLevel, StringView inCategory, std::format_string<Args...> inFormat, Args&&... inArgs)
{
    if (inLevel < g_Logger.GetMinimumLevel())
        return;

    g_Logger.Log(inLevel, inCategory, std::format(inFormat, std::forward<Args>(inArgs)...));
}

template<typename ...Args>
inline void gLogDebug(StringView inCategory, std::format_string<Args...> inFormat, Args&&... inArgs)
{
    gLog(LOG_LEVEL_DEBUG, inCategory, inFormat, std::forward<Args>(inArgs)...);
}

template<typename ...Args>
inline void gLogInfo(StringView inCategory, std::format_string<Args...> inFormat, Args&&... inArgs)
{
    gLog(LOG_LEVEL_INFO, inCategory, inFormat, std::forward<Args>(inArgs)...);
}

template<typename ...Args>
inline void gLogWarning(StringView inCategory, std::format_string<Args...> inFormat, Args&&... inArgs)
{
    gLog(LOG_LEVEL_WARNING, inCategory, inFormat, std::forward<Args>(inArgs)...);
}

template<typename ...Args>
inline void gLogError(StringView inCategory, std::format_string<Args...> inFormat, Args&&... inArgs)
{
    gLog(LOG_LEVEL_ERROR, inCategory, inFormat, std::forward<Args>(inArgs)...);
}

} // namespace RK
