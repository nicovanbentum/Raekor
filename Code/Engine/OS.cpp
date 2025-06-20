#include "pch.h"
#include "OS.h"

namespace RK {

#ifdef WIN32

void  OS::sOpenFile(const char* inFile)
{
	ShellExecute(NULL, "open", inFile, NULL, NULL, SW_RESTORE);
}


bool OS::sRunMsBuild(const char* args)
{
	LPSTARTUPINFOA startupInfo;
	PROCESS_INFORMATION procInfo;
	memset(&startupInfo, 0, sizeof(startupInfo));
	memset(&procInfo, 0, sizeof(procInfo));

	std::string cmdLine("C:\\Windows\\Microsoft.NET\\Framework64\\v4.0.30319\\MSBuild.exe ");
	cmdLine += args;

	if (!CreateProcessA(0, const_cast<char*>( cmdLine.c_str() ),
		0, 0, FALSE, 0, 0, 0, startupInfo, &procInfo))
		return false;
	WaitForSingleObject(procInfo.hProcess, INFINITE);
	DWORD dwExitCode;
	GetExitCodeProcess(procInfo.hProcess, &dwExitCode);
	CloseHandle(procInfo.hProcess);
	CloseHandle(procInfo.hThread);
	return dwExitCode == 0;
}


bool OS::sCreateProcess(const char* inCmd)
{
	STARTUPINFOA startupInfo = {};
	PROCESS_INFORMATION procInfo = {};

	if (!CreateProcessA(0, const_cast<char*>( inCmd ), 0, 0, FALSE, 0, 0, 0, &startupInfo, &procInfo))
		return false;

	WaitForSingleObject(procInfo.hProcess, INFINITE);
	
	DWORD dwExitCode;
	GetExitCodeProcess(procInfo.hProcess, &dwExitCode);
	
	CloseHandle(procInfo.hProcess);
	CloseHandle(procInfo.hThread);
	
	return dwExitCode == 0;
}


void OS::sCopyToClipboard(const char* inText)
{
	const size_t len = strlen(inText) + 1;
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
	memcpy(GlobalLock(hMem), inText, len);
	GlobalUnlock(hMem);
	OpenClipboard(0);
	EmptyClipboard();
	SetClipboardData(CF_TEXT, hMem);
	CloseClipboard();
}


bool OS::sSetDarkTitleBar(SDL_Window* inWindow)
{
    SDL_PropertiesID props = SDL_GetWindowProperties(inWindow);
    HWND hwnd = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

	BOOL dark = TRUE;
	DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
	DwmGetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
	return dark == TRUE;
}


void* OS::sGetFunctionPointer(const char* inName)
{
	return GetProcAddress(GetModuleHandleA(NULL), inName);
}


bool OS::sCheckCommandLineOption(const char* inOption)
{
    static const char* cmd_line = GetCommandLineA();
    static const size_t cmd_line_len = strlen(cmd_line);

    const char* substr = strstr(cmd_line, inOption);
    return substr != nullptr;
}


String OS::sGetCommandLineValue(const char* inOption)
{
	static const char* cmd_line = GetCommandLineA();
	static const size_t cmd_line_len = strlen(cmd_line);

	const char* substr = strstr(cmd_line, inOption);
	if (substr == nullptr)
		return {};

	const size_t option_len = strlen(inOption);
	const char* equals = substr + option_len;
	if (*equals  != '=')
		return {};

	char delim = ' ';
	const char* start = equals + 1;
	const char* end = start;

	if (*end == '"')
	{
		delim = '"';
		++start; ++end;
	}

	while (*end != delim && end != cmd_line + cmd_line_len)
		end++;

	return std::string(start, end);
}


String OS::sOpenFileDialog(const char* filters)
{
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

	if (GetOpenFileNameA(&ofn) == TRUE)
	{
		return ofn.lpstrFile;
	}
	return std::string();
}


String OS::sSaveFileDialog(const char* filters, const char* defaultExt)
{
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

	if (GetSaveFileNameA(&ofn) == TRUE)
	{
		return ofn.lpstrFile;
	}
	return std::string();
}


Path OS::sGetTempPath()
{
	char filepath[MAX_PATH];
	GetTempPathA(MAX_PATH, filepath);
	return fs::path(filepath);
}


Path OS::sGetExecutablePath()
{
	char filepath[MAX_PATH];
	GetModuleFileNameA(NULL, filepath, MAX_PATH);
	return fs::path(filepath);
}


Path OS::sGetExecutableDirectoryPath()
{
	return sGetExecutablePath().parent_path();
}


String OS::sSelectFolderDialog()
{
	IFileDialog* pfd;
	if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd))))
	{
		DWORD dwOptions;

		if (SUCCEEDED(pfd->GetOptions(&dwOptions)))
			pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);

		if (SUCCEEDED(pfd->Show(NULL)))
		{
			IShellItem* psi;

			if (SUCCEEDED(pfd->GetResult(&psi)))
			{
				LPWSTR result;

				if (SUCCEEDED(psi->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &result)))
				{
					char ch = ' ';
					char folder_path[MAX_PATH];
					WideCharToMultiByte(CP_ACP, 0, result, -1, folder_path, MAX_PATH, &ch, NULL);

					return folder_path;
				}

				psi->Release();
			}
		}
		pfd->Release();
	}
	return std::string();
}

#else 

std::string PlatformContext::openFileDialog(const std::vector<Ffilter>& filters)
{
	//init gtk
	m_assert(gtk_init_check(NULL, NULL), "failed to init gtk");
	// allocate a new dialog window
	auto dialog = gtk_file_chooser_dialog_new(
		"Open File",
		NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		"Open", GTK_RESPONSE_ACCEPT,
		NULL);

	for (auto& filter : filters)
	{
		auto gtk_filter = gtk_file_filter_new();
		gtk_file_filter_set_name(gtk_filter, filter.name.c_str());

		std::vector<std::string> patterns;
		std::string extensions = filter.extensions;
		size_t pos = 0;
		while (pos != std::string::npos)
		{
			pos = extensions.find(";");
			patterns.push_back(extensions.substr(0, pos));
			extensions.erase(0, pos + 1);
		}
		for (auto& pattern : patterns)
		{
			gtk_file_filter_add_pattern(gtk_filter, pattern.c_str());
		}
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), gtk_filter);
	}

	char* path = NULL;
	// if the ok button is pressed (gtk response type ACCEPT) we get the selected filepath
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	}
	// destroy our dialog window
	gtk_widget_destroy(dialog);
	// if our filepath is not empty we make it the return value
	std::string file;
	if (path)
	{
		file = std::string(path, strlen(path));
	}
	// main event loop for our window, this took way too long to fix 
	// (newer GTK's produce segfaults, something to do with SDL)
	while (gtk_events_pending())
	{
		gtk_main_iteration();
	}
	return file;
}

#endif

} // Namespace Raekor