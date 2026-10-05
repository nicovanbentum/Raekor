#include "PCH.h"
#include "Log.h"

namespace RK {

Logger g_Logger;


const char* gToString(ELogLevel inLevel)
{
    switch (inLevel)
    {
        case LOG_LEVEL_DEBUG:   return "Debug";
        case LOG_LEVEL_INFO:    return "Info";
        case LOG_LEVEL_WARNING: return "Warning";
        case LOG_LEVEL_ERROR:   return "Error";
        default:                return "Unknown";
    }
}


String gSerializeLogMessage(const LogMessage& inMessage)
{
    String data;
    data.reserve(1 + inMessage.mCategory.size() + 1 + inMessage.mText.size());

    data += char('0' + inMessage.mLevel);
    data += inMessage.mCategory;
    data += '\0';
    data += inMessage.mText;

    return data;
}


bool gDeserializeLogMessage(StringView inData, LogMessage& outMessage)
{
    if (inData.size() < 2)
        return false;

    const int level = inData[0] - '0';

    if (level < 0 || level >= LOG_LEVEL_COUNT)
        return false;

    const size_t separator = inData.find('\0', 1);

    if (separator == StringView::npos)
        return false;

    outMessage.mLevel = ELogLevel(level);
    outMessage.mCategory = String(inData.substr(1, separator - 1));
    outMessage.mText = String(inData.substr(separator + 1));

    return true;
}


Logger::Logger() : m_StartTime(std::chrono::steady_clock::now())
{
    m_History.reserve(cMaxHistorySize);

    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD console_mode = 0;

    if (console != INVALID_HANDLE_VALUE && GetConsoleMode(console, &console_mode))
        SetConsoleMode(console, console_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}


Logger::~Logger()
{
    CloseFile();
}


void Logger::Log(ELogLevel inLevel, StringView inCategory, StringView inText)
{
    if (inLevel < m_MinimumLevel)
        return;

    LogMessage message = LogMessage
    {
        .mLevel = inLevel,
        .mCategory = String(inCategory),
        .mText = String(inText),
        .mTime = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_StartTime).count(),
        .mThreadID = GetCurrentThreadId()
    };

    while (!message.mText.empty() && ( message.mText.back() == '\n' || message.mText.back() == '\r' ))
        message.mText.pop_back();

    std::scoped_lock lock(m_Mutex);

    WriteToConsole(message);
    WriteToFile(message);

    for (const auto& [sink_id, sink] : m_Sinks)
        sink(message);

    if (m_History.size() < cMaxHistorySize)
    {
        m_History.push_back(std::move(message));
    }
    else
    {
        m_History[m_HistoryStart] = std::move(message);
        m_HistoryStart = ( m_HistoryStart + 1 ) % cMaxHistorySize;
    }
}


uint32_t Logger::AddSink(const Sink& inSink, bool inReplayHistory)
{
    std::scoped_lock lock(m_Mutex);

    if (inReplayHistory)
    {
        for (size_t index = 0; index < m_History.size(); index++)
            inSink(m_History[( m_HistoryStart + index ) % m_History.size()]);
    }

    const uint32_t sink_id = m_NextSinkID++;
    m_Sinks.emplace_back(sink_id, inSink);
    return sink_id;
}


void Logger::RemoveSink(uint32_t inSinkID)
{
    std::scoped_lock lock(m_Mutex);
    std::erase_if(m_Sinks, [inSinkID](const Pair<uint32_t, Sink>& inSink) { return inSink.first == inSinkID; });
}


bool Logger::OpenFile(const Path& inPath)
{
    std::scoped_lock lock(m_Mutex);

    if (m_File.is_open())
        m_File.close();

    m_File.open(inPath, std::ios::out | std::ios::trunc);
    return m_File.is_open();
}


void Logger::CloseFile()
{
    std::scoped_lock lock(m_Mutex);

    if (m_File.is_open())
        m_File.close();
}


Array<LogMessage> Logger::GetHistory() const
{
    std::scoped_lock lock(m_Mutex);

    Array<LogMessage> history;
    history.reserve(m_History.size());

    for (size_t index = 0; index < m_History.size(); index++)
        history.push_back(m_History[( m_HistoryStart + index ) % m_History.size()]);

    return history;
}


void Logger::ClearHistory()
{
    std::scoped_lock lock(m_Mutex);

    m_History.clear();
    m_HistoryStart = 0;
}


void Logger::WriteToConsole(const LogMessage& inMessage)
{
    const char* color = "";

    switch (inMessage.mLevel)
    {
        case LOG_LEVEL_DEBUG:   color = "\033[0;90m"; break;
        case LOG_LEVEL_WARNING: color = "\033[0;33m"; break;
        case LOG_LEVEL_ERROR:   color = "\033[0;31m"; break;
    }

    const String line = std::format("{}[{:9.3f}] [{}] {}\033[0m\n", color, inMessage.mTime, inMessage.mCategory, inMessage.mText);

    std::FILE* stream = inMessage.mLevel >= LOG_LEVEL_WARNING ? stderr : stdout;
    std::fwrite(line.data(), 1, line.size(), stream);

    if (IsDebuggerPresent())
    {
        const String debug_line = std::format("[{}] [{}] {}\n", gToString(inMessage.mLevel), inMessage.mCategory, inMessage.mText);
        OutputDebugStringA(debug_line.c_str());
    }
}


void Logger::WriteToFile(const LogMessage& inMessage)
{
    if (!m_File.is_open())
        return;

    m_File << std::format("[{:9.3f}] [{:5}] [{}] [{}] {}\n", inMessage.mTime, inMessage.mThreadID, gToString(inMessage.mLevel), inMessage.mCategory, inMessage.mText);

    if (inMessage.mLevel >= LOG_LEVEL_WARNING)
        m_File.flush();
}

}
