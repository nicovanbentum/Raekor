#pragma once

#include "pch.h"

namespace Raekor {

/*  
    Interface for OS specific tasks.
    Compile time decides which function definitions to pull in.
*/
class OS {
public:
    static bool sRunMsBuild(const char* args);
    static void CopyToClipboard(const char* inText);
    static bool sSetDarkTitleBar(SDL_Window* inWindow);
    static std::string sOpenFileDialog(const char* filters);
    static std::string sSaveFileDialog(const char* filters, const char* defaultExt);
    static std::string sSelectFolderDialog();
};

} // Namespace Raekor
