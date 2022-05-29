#include "pch.h"
#include "rtti.h"
#include "util.h"

namespace Raekor {

RTTI::RTTI(const char* name) : m_NameHash(gHash32Bit(name)) {}

} // namespace Raekor

