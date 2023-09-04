#include "pch.h"
#include "util.h"

namespace Raekor {

bool SDL_IsWindowBorderless(SDL_Window* inWindow)
{
	const auto window_flags = SDL_GetWindowFlags(inWindow);
	return ( ( window_flags & SDL_WINDOW_FULLSCREEN_DESKTOP ) == SDL_WINDOW_FULLSCREEN_DESKTOP );
}


bool SDL_IsWindowExclusiveFullscreen(SDL_Window* inWindow)
{
	const auto window_flags = SDL_GetWindowFlags(inWindow);
	return ( ( window_flags & SDL_WINDOW_FULLSCREEN ) && !SDL_IsWindowBorderless(inWindow) );
}

static constexpr auto sProgressStrings = std::array {
	"----------",
	"#---------",
	"##--------",
	"###-------",
	"####------",
	"#####-----",
	"######----",
	"#######---",
	"########--",
	"#########-",
	"##########"
};

const char* gAsciiProgressBar(float inFraction)
{
	assert(inFraction >= 0.0f && inFraction <= 1.0f);
	return sProgressStrings[int(inFraction * 10)];
}


FileWatcher::FileWatcher(const std::string& path) : m_Path(path)
{
	m_LastWriteTime = std::filesystem::last_write_time(path);
}


bool FileWatcher::WasModified()
{
	if (auto new_time = std::filesystem::last_write_time(m_Path); new_time != m_LastWriteTime)
	{
		m_LastWriteTime = new_time;
		return true;
	}
	return false;
}

} // raekor
