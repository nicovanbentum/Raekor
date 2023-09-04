#pragma once

#include "pch.h"

namespace Raekor::OS {
/*
	Interface for OS specific tasks.
	Compile time decides which function definitions to pull in.
*/

enum class EDirectoryChange
{
	FILE_CREATED
};

bool sWatchDirectory(const Path& inDirPath, EDirectoryChange& outChange, Path& outFilePath);

bool  sRunMsBuild(const char* args);
void  sCopyToClipboard(const char* inText);
bool  sSetDarkTitleBar(SDL_Window* inWindow);
void* sGetFunctionPointer(const char* inName);
bool  sCheckCommandLineOption(const char* inOption);

std::string sOpenFileDialog(const char* filters);
std::string sSaveFileDialog(const char* filters, const char* defaultExt);
std::string sSelectFolderDialog();

fs::path sGetExecutablePath();
fs::path sGetExecutableDirectoryPath();

} // namespace Raekor::OS
