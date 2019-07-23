#pragma once

#include "pch.h"

namespace Raekor {

enum class renderAPI {
	OPENGL, DIRECTX11
};

class Application {

public:
	Application() {}

	void run();

	static inline renderAPI getActiveRenderAPI() { return active_render_api; }
	inline void setActiveRenderAPI(const renderAPI render_api) { active_render_api = render_api; }

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

	static renderAPI active_render_api;
};

} // Namespace Raekor