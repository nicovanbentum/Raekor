#include "pch.h"
#include "app.h"
#include "util.h"
#include "model.h"
#include "entry.h"
#include "camera.h"
#include "shader.h"
#include "framebuffer.h"
#include "PlatformContext.h"

namespace Raekor {

renderAPI Application::active_render_api = renderAPI::OPENGL;

void Application::serialize_settings(const std::string& filepath, bool write) {
	if (write) {
		std::ofstream os(filepath);
		cereal::JSONOutputArchive archive(os);
		serialize(archive);
	} else {
		std::ifstream is(filepath);
		cereal::JSONInputArchive archive(is);
		serialize(archive);
	}
}

void Raekor::Application::run() {
	auto context = Raekor::PlatformContext();

	// retrieve the application settings from the config file
	serialize_settings("config.json");

	IDXGISwapChain* swap_chain;
	ID3D11Device* d3device;
	ID3D11DeviceContext* d3context;
	DXGI_SWAP_CHAIN_DESC sc_desc;
	memset(&sc_desc, 0, sizeof(DXGI_SWAP_CHAIN_DESC));

	m_assert(SDL_Init(SDL_INIT_VIDEO) == 0, "failed to init sdl");

	const char* glsl_version = "#version 330";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	Uint32 wflags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
		SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED;
	Uint32 d3wflags = SDL_WINDOW_RESIZABLE |
		SDL_WINDOW_ALLOW_HIGHDPI;


	std::vector<SDL_Rect> displays;
	for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
		displays.push_back(SDL_Rect());
		SDL_GetDisplayBounds(i, &displays.back());
	}
	// if our display setting is higher than the nr of displays we pick the default display
	auto index = display > displays.size() - 1 ? 0 : display;
	auto main_window = SDL_CreateWindow(name.c_str(),
		displays[index].x,
		displays[index].y,
		displays[index].w,
		displays[index].h,
		wflags);

	auto directxwindow = SDL_CreateWindow("Raekor (DirectX 11)", 100, 100, 1280, 720, d3wflags);

	// query for the sdl window hardware handle for our directx renderer
	SDL_SysWMinfo wminfo;
	SDL_VERSION(&wminfo.version);
	SDL_GetWindowWMInfo(directxwindow, &wminfo);
	auto dx_hwnd = wminfo.info.win.window;

	// fill out the swap chain description and create both the device and swap chain
	sc_desc.BufferCount = 1;
	sc_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sc_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sc_desc.OutputWindow = dx_hwnd;
	sc_desc.SampleDesc.Count = 4;
	sc_desc.Windowed = TRUE;
	D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, NULL, NULL, NULL, D3D11_SDK_VERSION, &sc_desc, &swap_chain, &d3device, NULL, &d3context);

	// setup a directx back buffer to draw to
	ID3D11RenderTargetView* d3_backbuffer;
	ID3D11Texture2D* backbuffer_addr;
	swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backbuffer_addr);
	d3device->CreateRenderTargetView(backbuffer_addr, NULL, &d3_backbuffer);
	backbuffer_addr->Release();
	d3context->OMSetRenderTargets(1, &d3_backbuffer, NULL);

	// set the directx viewport
	D3D11_VIEWPORT viewport;
	memset(&viewport, 0, sizeof(D3D11_VIEWPORT));
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = 1280;
	viewport.Height = 720;
	d3context->RSSetViewports(1, &viewport);

	// error handling result
	HRESULT hr;

	// com pointers to direct3d11 variables
	Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;
	Microsoft::WRL::ComPtr<ID3D10Blob> vertex_shader_buffer;
	Microsoft::WRL::ComPtr<ID3D10Blob> pixel_shader_buffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> index_buffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterize_state;

	// struct that describes and matches what the hlsl vertex shader expects
	struct cb_vs {
		glm::mat4 MVP;
	};

	// fill out the constant buffer description for our MVP struct
	D3D11_BUFFER_DESC cbdesc;
	cbdesc.Usage = D3D11_USAGE_DYNAMIC;
	cbdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbdesc.MiscFlags = 0;
	cbdesc.ByteWidth = static_cast<UINT>(sizeof(cb_vs) + (16 - (sizeof(cb_vs) % 16)));
	cbdesc.StructureByteStride = 0;

	// fill out the raster description struct and create a rasterizer state
	D3D11_RASTERIZER_DESC raster_desc;
	memset(&raster_desc, 0, sizeof(D3D11_RASTERIZER_DESC));
	raster_desc.FillMode = D3D11_FILL_MODE::D3D11_FILL_SOLID;
	raster_desc.CullMode = D3D11_CULL_MODE::D3D11_CULL_NONE;
	raster_desc.FrontCounterClockwise = false;
	d3device->CreateRasterizerState(&raster_desc, rasterize_state.GetAddressOf());

	// read our compiled shader files and put them in their respective buffers
	hr = D3DReadFileToBlob(L"shaders/simple_vertex.cso", vertex_shader_buffer.GetAddressOf());
	if (FAILED(hr)) m_assert(false, "failed to load d3 shader");
	hr = D3DReadFileToBlob(L"shaders/simple_pixel.cso", pixel_shader_buffer.GetAddressOf());
	if (FAILED(hr)) m_assert(false, "failed to load d3 shader");

	hr = d3device->CreateVertexShader(vertex_shader_buffer.Get()->GetBufferPointer(), vertex_shader_buffer->GetBufferSize(), NULL, vertex_shader.GetAddressOf());
	if (FAILED(hr)) m_assert(false, "failed to create vertex shader");
	hr = d3device->CreatePixelShader(pixel_shader_buffer.Get()->GetBufferPointer(), pixel_shader_buffer->GetBufferSize(), NULL, pixel_shader.GetAddressOf());
	if (FAILED(hr)) m_assert(false, "failed to create pixel shader");

	// describe our vertex layout
	D3D11_INPUT_ELEMENT_DESC layout[] = { "POSITION", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA, 0 };
	hr = d3device->CreateInputLayout(layout, ARRAYSIZE(layout), vertex_shader_buffer->GetBufferPointer(), vertex_shader_buffer->GetBufferSize(), input_layout.GetAddressOf());
	if (FAILED(hr)) m_assert(false, "failed to create pinput layout");

	// get our OpenGL context and make it current for every openGL call
	SDL_GLContext gl_context = SDL_GL_CreateContext(main_window);
	SDL_GL_MakeCurrent(main_window, gl_context);
	SDL_SetWindowResizable(main_window, SDL_FALSE);

	// init gl3w so we can use openGL
	m_assert(gl3wInit() == 0, "failed to init gl3w");

	// print the initialized openGL specs
	std::cout << "GL INFO: OpenGL " << glGetString(GL_VERSION);
	printf("GL INFO: OpenGL %s, GLSL %s\n", glGetString(GL_VERSION),
		glGetString(GL_SHADING_LANGUAGE_VERSION));

	// enumerate all avaiable graphics adapters
	std::vector<IDXGIAdapter*> GPUs;
	IDXGIFactory* gpu_factory = NULL;
	IDXGIAdapter* gpu;
	CreateDXGIFactory1(__uuidof(IDXGIFactory), (void**)&gpu_factory);
	for (unsigned int i = 0; gpu_factory->EnumAdapters(i, &gpu) != DXGI_ERROR_NOT_FOUND; i++) {
		GPUs.push_back(gpu);
	}

	// print every graphics adapters description
	for (IDXGIAdapter* adapter : GPUs) {
		DXGI_ADAPTER_DESC description;
		adapter->GetDesc(&description);
		std::wcout << description.Description << std::endl;
	}

	// test cube for directx rendering
	std::unique_ptr<Raekor::Mesh> mcube;
	mcube.reset(new Raekor::Mesh("resources/models/testcube.obj", Raekor::Mesh::file_format::OBJ));

	// fill out the buffer description struct for our vertex buffer
	D3D11_BUFFER_DESC vb_desc = { 0 };
	vb_desc.Usage = D3D11_USAGE_DEFAULT;
	vb_desc.ByteWidth = static_cast<UINT>(sizeof(glm::vec3) * mcube->vertices.size());
	vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vb_desc.CPUAccessFlags = 0;
	vb_desc.MiscFlags = 0;

	// create the buffer that actually holds our vertices
	D3D11_SUBRESOURCE_DATA vb_data = { 0 };
	vb_data.pSysMem = &(mcube->vertices[0]);
	hr = d3device->CreateBuffer(&vb_desc, &vb_data, vertex_buffer.GetAddressOf());
	if (FAILED(hr)) m_assert(false, "failed to create buffer");

	// create our index buffer
	// fill out index buffer description
	D3D11_BUFFER_DESC ib_desc = {0};
	ib_desc.Usage = D3D11_USAGE_DEFAULT;
	ib_desc.ByteWidth = static_cast<UINT>(sizeof(unsigned int) * mcube->indices.size());
	ib_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ib_desc.CPUAccessFlags = 0;
	ib_desc.MiscFlags = 0;

	// create a data buffer using our mesh's indices vector data
	D3D11_SUBRESOURCE_DATA ib_data;
	ib_data.pSysMem = &(mcube->indices[0]);
	hr = d3device->CreateBuffer(&ib_desc, &ib_data, index_buffer.GetAddressOf());
	if (FAILED(hr)) m_assert(false, "failed to create index buffer");

	// initialize ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImFont* pFont = io.Fonts->AddFontFromFileTTF(font.c_str(), 18.0f);
	if (!io.Fonts->Fonts.empty()) {
		io.FontDefault = io.Fonts->Fonts.back();
	}
	ImGui::StyleColorsDark();
	ImGui_ImplSDL2_InitForOpenGL(main_window, &gl_context);
	ImGui_ImplOpenGL3_Init(glsl_version);

	// print GPU information
	auto vendor = glGetString(GL_VENDOR);
	auto renderer = glGetString(GL_RENDERER);
	std::cout << vendor << " : " << renderer << std::endl;

	// set opengl depth testing and culling
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	//glEnable(GL_CULL_FACE);

	// one time setup for binding vertey arrays
	GLuint vertex_array_id;
	glGenVertexArrays(1, &vertex_array_id);
	glBindVertexArray(vertex_array_id);

	// load our shaders and get the MVP handles
	auto simple_shader = Raekor::GLShader("shaders/simple.vert", "shaders/simple.frag");
	auto skybox_shader = Raekor::GLShader("shaders/skybox.vert", "shaders/skybox.frag");
	auto simple_shader_mvp = simple_shader.get_uniform_location("MVP");
	auto skybox_shader_mvp = skybox_shader.get_uniform_location("MVP");

	// create a Camera we can use to move around our scene
	Raekor::Camera camera(glm::vec3(0, 0, 5), 45.0f);

	// Setup our scene TODO: create a class for this that's easy to serialize
	std::vector<Raekor::TexturedModel> scene;
	int active_model = 0;

	// persistent imgui variable values
	std::string mesh_file = "";
	std::string texture_file = "";
	float last_input_scale = 1.0f;
	glm::vec3 last_pos(0.0f);
	auto active_skybox = skyboxes.begin();

	// frame buffer for the renderer
	std::unique_ptr<Raekor::FrameBuffer> frame_buffer;
	frame_buffer.reset(Raekor::FrameBuffer::construct(glm::vec2(1280, 720)));

	// loads a cube mesh and a texture image to use as skybox
	std::unique_ptr<Raekor::GLTextureCube> skybox_texture;
	skybox_texture.reset(new Raekor::GLTextureCube(active_skybox->second));
	std::unique_ptr<Raekor::Mesh> skycube;
	skycube.reset(new Raekor::Mesh("resources/models/ikea box.obj", Raekor::Mesh::file_format::OBJ));

	SDL_SetWindowInputFocus(main_window);

	//main application loop
	for (;;) {
		//handle sdl and imgui events
		handle_sdl_gui_events({ main_window, directxwindow }, camera);

		// clear the render view
		const float clear_color[4] = { 0.0f, 0.2f, 0.0f, 1.0f };
		d3context->ClearRenderTargetView(d3_backbuffer, clear_color);
		// set the input layout, topology, rasterizer state and bind our vertex and pixel shader
		d3context->IASetInputLayout(input_layout.Get());
		d3context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		d3context->RSSetState(rasterize_state.Get());
		d3context->VSSetShader(vertex_shader.Get(), NULL, 0);
		d3context->PSSetShader(pixel_shader.Get(), NULL, 0);
		
		// set our constant buffer data containing the MVP of our mesh/model
		cb_vs data;
		mcube->euler_rotation.y = 0.01f;
		mcube->rotate(mcube->get_rotation_matrix());
		data.MVP = camera.get_dx_mvp(mcube->get_transformation());

		// create a directx resource for our MVP data
		D3D11_SUBRESOURCE_DATA cbdata;
		cbdata.pSysMem = &data;
		cbdata.SysMemPitch = 0;
		cbdata.SysMemSlicePitch = 0;
		auto ret = d3device->CreateBuffer(&cbdesc, &cbdata, constant_buffer.GetAddressOf());
		if (FAILED(ret)) m_assert(false, "failed to create constant buffer");

		// bind our constant, vertex and index buffers
		constexpr unsigned int stride = sizeof(glm::vec3);
		constexpr unsigned int offset = 0;
		d3context->VSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
		d3context->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);
		d3context->IASetIndexBuffer(index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		// draw the indexed vertices and swap the backbuffer to front
		d3context->DrawIndexed(static_cast<UINT>(mcube->indices.size()), 0, 0);
		swap_chain->Present(0, NULL);

		// clear opengl window background (blue)
		glClearColor(0.22f, 0.32f, 0.42f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// match our GL viewport with the framebuffer's size and clear it
		auto fsize = frame_buffer->get_size();
		glViewport(0, 0, (GLsizei)fsize.x, (GLsizei)fsize.y);
		frame_buffer->bind();
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// bind our skybox shader, update our camera WITHOUT translation (important for skybox)
		// upload our MVP, bind the skybox texture and render the entire thing
		glDepthMask(GL_FALSE);
		skybox_shader.bind();
		camera.update();
		skybox_shader.upload_uniform_matrix4fv(skybox_shader.get_uniform_location("MVP"), &camera.get_mvp()[0][0]);
		skybox_texture->bind();
		skycube->render();
		skybox_texture->unbind();
		glDepthMask(GL_TRUE);

		// bind our simple shader and draw textured model in our scene
		simple_shader.bind();
		for (auto& m : scene) {
			m.get_mesh()->rotate(m.get_mesh()->get_rotation_matrix());
			camera.update(m.get_mesh()->get_transformation());
			simple_shader.upload_uniform_matrix4fv(simple_shader.get_uniform_location("MVP"), &camera.get_mvp()[0][0]);
			m.render();
		}
		frame_buffer->unbind();
		simple_shader.unbind();

		//get new frame for opengl, sdl and imgui
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(main_window);
		ImGui::NewFrame();

		// draw the top level bar that contains stuff like "File" and "Edit"
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("Close", "CTRL+S")) {
					//clean up imgui
					ImGui_ImplOpenGL3_Shutdown();
					ImGui_ImplSDL2_Shutdown();
					ImGui::DestroyContext();

					//clean up SDL
					SDL_GL_DeleteContext(gl_context);
					SDL_DestroyWindow(main_window);
					SDL_Quit();
					return;
				}
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}

		// renderer viewport
		ImGui::SetNextWindowContentSize(ImVec2(fsize.x, fsize.y));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("Renderer", NULL, ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::Image((void*)static_cast<size_t>(frame_buffer->get_frame()), ImVec2(fsize.x, fsize.y), { 0,1 }, { 1,0 });
		ImGui::End();
		ImGui::PopStyleVar();

		// scene panel
		ImGui::SetNextWindowSize(ImVec2(760, 260), ImGuiCond_Once);
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 770, io.DisplaySize.y - 550), ImGuiCond_Once);
		ImGui::Begin("Scene");
		if (ImGui::BeginCombo("Skybox", active_skybox->first.c_str())) {
			for (auto it = skyboxes.begin(); it != skyboxes.end(); it++) {
				bool selected = (it == active_skybox);
				if (ImGui::Selectable(it->first.c_str(), selected)) {
					active_skybox = it;
					skybox_texture.reset(new Raekor::GLTextureCube(active_skybox->second));
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		// combo box used to select the mesh you're trying to edit
		if (ImGui::BeginCombo("", mesh_file.c_str())) {
			for (int i = 0; i < scene.size(); i++) {
				bool selected = (i == active_model);

				//hacky way to give the selectable a unique ID
				auto name = scene[i].get_mesh()->mesh_path;
				name = name + "##" + std::to_string(i);

				if (ImGui::Selectable(name.c_str(), selected)) {
					active_model = i;
					mesh_file = Raekor::get_extension(scene[active_model].get_mesh()->mesh_path);
					if (scene[active_model].get_texture() != nullptr) {
						texture_file = Raekor::get_extension(scene[active_model].get_texture()->get_path());
					}
					else {
						texture_file = "";
					}
					last_input_scale = scene[active_model].get_mesh()->scale;
					last_pos = scene[active_model].get_mesh()->position;
				}

				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		// plus button that opens a file dialog window using the platform context to ask what model file you want to load in
		ImGui::SameLine();
		if (ImGui::Button("+")) {
			std::string path = context.open_file_dialog({ ".obj" });
			if (!path.empty()) {
				scene.push_back(Raekor::TexturedModel(path, ""));
				active_model = (int)scene.size() - 1;
				mesh_file = Raekor::get_extension(scene[active_model].get_mesh()->mesh_path);
				texture_file = "";
			}
		}

		// minus button that removes the active model
		ImGui::SameLine();
		if (ImGui::Button("-") && !scene.empty()) {
			scene.erase(scene.begin() + active_model);
			active_model = 0;

			// if we just deleted the last model, empty the strings
			if (scene.empty()) {
				mesh_file = "";
				texture_file = "";
			}
			else {
				mesh_file = Raekor::get_extension(scene[active_model].get_mesh()->mesh_path);
				if (scene[active_model].get_texture() != nullptr) {
					texture_file = Raekor::get_extension(scene[active_model].get_texture()->get_path());
				}
				else {
					texture_file = "";
				}

			}
		}

		// toggle button for openGl vsync
		static bool is_vsync = true;
		if (ImGui::RadioButton("USE VSYNC", is_vsync)) {
			is_vsync = !is_vsync;
			SDL_GL_SetSwapInterval(is_vsync);
		}
		ImGui::End();


		// if we have at least 1 model in our scene we draw the panel that controls the models' variables
		if (!scene.empty()) {
			//start drawing a new imgui window. TODO: make this into a reusable component
			ImGui::SetNextWindowSize(ImVec2(760, 260), ImGuiCond_Once);
			ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 770, io.DisplaySize.y - 270), ImGuiCond_Once);
			ImGui::Begin("Object Properties");

			// opens a file dialog to select a new mesh for the model
			ImGui::Text("Mesh");
			ImGui::Text(mesh_file.c_str(), NULL);
			ImGui::SameLine();
			if (ImGui::Button("...##Mesh")) {
				std::string path = context.open_file_dialog({ ".obj" });
				if (!path.empty()) {
					scene[active_model].set_mesh(path);
					mesh_file = Raekor::get_extension(scene[active_model].get_mesh()->mesh_path);

				}
			}
			ImGui::Separator();

			// opens a file dialog to select a new texture for the model
			ImGui::Text("Texture");
			ImGui::Text(texture_file.c_str(), NULL);
			ImGui::SameLine();
			if (ImGui::Button("...##Texture")) {
				std::string path = context.open_file_dialog({ ".png" });
				if (!path.empty()) {
					scene[active_model].set_texture(path);
					texture_file = Raekor::get_extension(scene[active_model].get_texture()->get_path());
				}
			}

			// input float that lets you control the scale of the model
			if (ImGui::InputFloat("Scale", &scene[active_model].get_mesh()->scale, 0.05f, 0.1f, "%.2f")) {
				glm::vec3 factor(scene[active_model].get_mesh()->scale / last_input_scale);
				scene[active_model].get_mesh()->scale_by(factor);
				last_input_scale = scene[active_model].get_mesh()->scale;
			}

			// rotation is continuous for now so we have something to animate
			if (ImGui::SliderFloat3("Rotate", &scene[active_model].get_mesh()->euler_rotation.x, -0.10f, 0.10f, "%.3f")) {}

			// input float that controls the placement of the model in the scene
			if (ImGui::InputFloat3("Move", &scene[active_model].get_mesh()->position.x, 2)) {
				glm::vec3 delta = scene[active_model].get_mesh()->position - last_pos;
				scene[active_model].get_mesh()->move(delta);
				last_pos = scene[active_model].get_mesh()->position;
			}

			// resets the model's transformation
			if (ImGui::Button("Reset")) {
				last_input_scale = 1.0f;
				last_pos = glm::vec3(0.0f);
				scene[active_model].get_mesh()->reset_transformation();
			}
			ImGui::End();
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		int w, h;
		SDL_GetWindowSize(main_window, &w, &h);
		glViewport(0, 0, w, h);

		SDL_GL_SwapWindow(main_window);
	}

	// directx cleanup
	swap_chain->Release();
	d3device->Release();
	d3context->Release();
	d3_backbuffer->Release();
}
 
} // namespace Raekor