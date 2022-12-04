#include "pch.h"
#include "cvars.h"

namespace Raekor {
	CVars::CVars() {
		auto stream = std::ifstream("cvars.json");
		if (!stream.is_open())
			return;

		auto archive = cereal::JSONInputArchive(stream);
		archive(cvars);
	}


	CVars::~CVars() {
		auto stream = std::ofstream("cvars.json");
		auto archive = cereal::JSONOutputArchive(stream);
		archive(cvars);
	}


	std::string CVars::sGetValue(const std::string& name) {
		if (global->cvars.find(name) == global->cvars.end())
			return {};

		try { return std::visit(GetVisitor(), global->cvars[name]); }
		catch (std::exception& e) {
			UNREFERENCED_PARAMETER(e);
			return {};
		}
	}
}