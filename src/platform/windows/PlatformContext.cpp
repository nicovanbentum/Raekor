#include "pch.h"
#include "PlatformContext.h"

namespace Raekor {

std::string PlatformContext::open_file_dialog(const std::vector<std::string>& filters) {
	std::string lpstr_filters;
	for (auto& f : filters) {
		lpstr_filters.append(std::string(f + '\0' + '*' + f + '\0'));
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