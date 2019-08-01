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
	virtual void ImGui_render()							= 0;
	virtual void clear(glm::vec4 color)					= 0;
	virtual void draw_indexed(unsigned int size)		= 0;
	virtual void ImGui_new_frame(SDL_Window* window)	= 0;

private:
	static RenderAPI activeAPI;
};

class GLRenderer : public Renderer {
public:
	GLRenderer(SDL_Window* window);
	~GLRenderer();

	virtual void ImGui_render()							override;
	virtual void clear(glm::vec4 color)					override;
	virtual void draw_indexed(unsigned int size)		override;
	virtual void ImGui_new_frame(SDL_Window* window)	override;
private:
	SDL_GLContext context;
};

} // namespace Raekor