#pragma once

#include "pch.h"

namespace Raekor {

	class PlatformContext {

	public:
		PlatformContext() {}
		std::string open_file_dialog(const std::vector<std::string>& filters);
	};

} // Namespace Raekor
