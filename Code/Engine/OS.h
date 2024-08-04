#pragma once

#include "pch.h"

namespace RK::OS {
/*
	Interface for OS specific tasks.
	Compile time decides which function definitions to pull in.
*/

enum class EDirectoryChange
{
	FILE_CREATED
};

bool sWatchDirectory(const Path& inDirPath, EDirectoryChange& outChange, Path& outFilePath);

void  sOpenFile(const char* inFile);
bool  sRunMsBuild(const char* args);
bool  sCreateProcess(const char* inCmd);
void  sCopyToClipboard(const char* inText);
bool  sSetDarkTitleBar(SDL_Window* inWindow);
void* sGetFunctionPointer(const char* inName);

bool  sCheckCommandLineOption(const char* inOption);
String sGetCommandLineValue(const char* inOption);

String sOpenFileDialog(const char* filters);
String sSaveFileDialog(const char* filters, const char* defaultExt);
String sSelectFolderDialog();

Path sGetTempPath();
Path sGetExecutablePath();
Path sGetExecutableDirectoryPath();

} // namespace Raekor::OS
