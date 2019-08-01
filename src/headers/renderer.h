#pragma once

#include "pch.h"
#include "util.h"
#include "mesh.h"


namespace Raekor {

enum class RenderAPI {
	OPENGL, DIRECTX11
};

class Renderer {
public:
	// static constructor that abstracts the runtime renderer construction
	static Renderer* construct(SDL_Window* window);

	// functions for getting and setting the active render API
	inline static RenderAPI get_activeAPI() { return activeAPI; }
	inline static void set_activeAPI(const RenderAPI new_active) { activeAPI = new_active; }

	// interface functions implemented per API
	virtual void clear(glm::vec4 color) = 0;
	virtual void GUI_new_frame(SDL_Window* window) = 0;
	virtual void GUI_render() = 0;
	virtual void render(const Raekor::Mesh& m) = 0;

private:
	static RenderAPI activeAPI;
};

class GLRenderer : public Renderer {
public:
	GLRenderer(SDL_Window* window);
	~GLRenderer();
	virtual void clear(glm::vec4 color) override;
	virtual void GUI_new_frame(SDL_Window* window) override;
	virtual void GUI_render() override;
	virtual void render(const Raekor::Mesh& m) override;
private:
	SDL_GLContext context;
};

} // namespace Raekor