#include "pch.h"
#include "../OS.h"
#include "util.h"

namespace Raekor {

bool OS::RunMsBuild(const char* args) {
    LPSTARTUPINFOA startupInfo;
    PROCESS_INFORMATION procInfo;
    memset(&startupInfo, 0, sizeof(startupInfo));
    memset(&procInfo, 0, sizeof(procInfo));

    std::string cmdLine("C:\\Windows\\Microsoft.NET\\Framework64\\v4.0.30319\\MSBuild.exe ");
    cmdLine += args;

    if (!CreateProcessA(0, const_cast<char*>(cmdLine.c_str()),
        0, 0, FALSE, 0, 0, 0, startupInfo, &procInfo))
        return false;
    WaitForSingleObject(procInfo.hProcess, INFINITE);
    DWORD dwExitCode;
    GetExitCodeProcess(procInfo.hProcess, &dwExitCode);
    CloseHandle(procInfo.hProcess);
    CloseHandle(procInfo.hThread);
    return dwExitCode == 0;
}

std::string OS::openFileDialog(const char* filters) {

    OPENFILENAMEA ofn;
    CHAR szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filters;
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

std::string OS::saveFileDialog(const char* filters, const char* defaultExt) {
    OPENFILENAMEA ofn;
    CHAR szFile[260] = { 'u', 'n', 't', 'i', 't', 'l', 'e', 'd', '\0' };
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filters;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.lpstrDefExt = defaultExt;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&ofn) == TRUE) {
        return ofn.lpstrFile;
    }
    return std::string();
}

} // Namespace Raekor