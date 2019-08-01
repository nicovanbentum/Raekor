#pragma once

#include "pch.h"

namespace Raekor {

class Application {

public:
	Application() {}

	void run();

	template<class C>
	void serialize(C& archive) {
		archive(
			CEREAL_NVP(name),
			CEREAL_NVP(display),
			CEREAL_NVP(font),
			CEREAL_NVP(skyboxes));
	}
	void serialize_settings(const std::string& filepath, bool write = false);


private:
	std::string name;
	std::string font;
	uint8_t display;
	std::map<std::string, std::vector<std::string>> skyboxes;
};

} // Namespace Raekor