#pragma once

#include "pch.h"

namespace Raekor {

struct Ffilter {
    std::string name;
    std::string extensions;
};

// Interface for OS specific tasks
// Compile time decides which function definitions to pull in
class OS {
public:
    static std::string openFileDialog(const std::vector<Ffilter>& filters);
    static std::string saveFileDialog(const char* filters, const char* defaultExt);
};

} // Namespace Raekor
