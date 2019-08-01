#include "pch.h"
#include "renderer.h"

#ifdef _WIN32
	#include "DXRenderer.h"
#endif

namespace Raekor {

// global enum for changing the active render API
RenderAPI Renderer::activeAPI = RenderAPI::OPENGL;

Renderer* Renderer::construct(SDL_Window* window) {
	switch (activeAPI) {
		case RenderAPI::OPENGL: {
			return new GLRenderer(window);
		} break;
#ifdef _WIN32
		case RenderAPI::DIRECTX11: {
			return new DXRenderer(window);
		} break;
#endif
	}
	return nullptr;
}

GLRenderer::GLRenderer(SDL_Window* window) {
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	
	context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, context);
	m_assert(gl3wInit() == 0, "failed to init gl3w");
	
	// print the initialized openGL specs
	std::cout << "GL INFO: OpenGL " << glGetString(GL_VERSION);
	printf("GL INFO: OpenGL %s, GLSL %s\n", glGetString(GL_VERSION),
		glGetString(GL_SHADING_LANGUAGE_VERSION));
	
	ImGui_ImplSDL2_InitForOpenGL(window, &context);
	ImGui_ImplOpenGL3_Init("#version 330");

	// set opengl depth testing and culling
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	//glEnable(GL_CULL_FACE);

	// one time setup for binding vertey arrays
	unsigned int vertex_array_id;
	glGenVertexArrays(1, &vertex_array_id);
	glBindVertexArray(vertex_array_id);
}

GLRenderer::~GLRenderer() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	SDL_GL_DeleteContext(context);
}

void GLRenderer::clear(glm::vec4 color) {
	glClearColor(color.r, color.g, color.b, color.a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLRenderer::GUI_new_frame(SDL_Window* window) {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame(window);
	ImGui::NewFrame();
}

void GLRenderer::GUI_render() {
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GLRenderer::render(const Raekor::Mesh& m) {

}


} // namespace Raekor