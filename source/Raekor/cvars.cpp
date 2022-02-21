#include "pch.h"
#include "cvars.h"

namespace Raekor {
	CVars::CVars() {
		std::ifstream stream("cvars.json");

		if (!stream.is_open()) {
			return;
		}

		cereal::JSONInputArchive archive(stream);
		archive(cvars);
	}


	CVars::~CVars() {
		std::ofstream stream("cvars.json");
		cereal::JSONOutputArchive archive(stream);
		archive(cvars);
	}


	std::string CVars::sGetValue(const std::string& name) {
		if (global->cvars.find(name) == global->cvars.end()) {
			return {};
		}

		try {
			GetVisitor visitor;
			return std::visit(visitor, global->cvars[name]);
		}
		catch (std::exception& e) {
			UNREFERENCED_PARAMETER(e);
			return {};
		}
	}
}