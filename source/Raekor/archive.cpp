#include "pch.h"
#include "archive.h"
#include "rtti.h"

namespace Raekor::JSON {

ReadArchive::ReadArchive(const Path& inPath) {
	auto ifs = std::ifstream(inPath);
	std::stringstream buffer;
	buffer << ifs.rdbuf();
	m_Parser = JSON::Parser(buffer.str());
	m_Parser.Parse();
}


WriteArchive::WriteArchive(const Path& inPath) {
	m_Ofs = std::ofstream(inPath);
}

}
