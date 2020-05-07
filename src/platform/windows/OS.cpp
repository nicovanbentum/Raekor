#include "pch.h"
#include "OS.h"
#include "util.h"

namespace Raekor {

std::string OS::openFileDialog(const std::vector<Ffilter>& filters) {
    std::string lpstr_filters;
    for (const Ffilter& filter : filters) {
        lpstr_filters.append(filter.name + " (" + filter.extensions + ')' + '\0' + filter.extensions + '\0');
    }

    OPENFILENAMEA ofn;
    CHAR szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = lpstr_filters.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return ofn.lpstrFile;
    }
    return std::string();
}

} // Namespace Raekor