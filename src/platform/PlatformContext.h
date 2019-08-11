#pragma once

#include "pch.h"

namespace Raekor {

// Interface for OS specific tasks
// Compile time decides which function definitions to pull in
class PlatformContext {
public:
    PlatformContext() {}
    std::string open_file_dialog(const std::vector<std::string>& filters);
};

} // Namespace Raekor
