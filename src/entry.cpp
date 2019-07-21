#include "pch.h"
#include "mesh.h"
#include "math.h"
#include "util.h"
#include "entry.h"
#include "model.h"
#include "camera.h"
#include "shader.h"
#include "framebuffer.h"
#include "PlatformContext.h"

struct settings {
    std::string name;
    std::string font;
    uint8_t display;
	std::string skybox;
	std::map<std::string, std::vector<std::string>> skyboxes;

    template<class C>
    void read_or_write(C& archive) {
        archive(
            CEREAL_NVP(name), 
            CEREAL_NVP(display), 
            CEREAL_NVP(font),
			CEREAL_NVP(skybox),
			CEREAL_NVP(skyboxes));
    }

    void read_from_disk(const std::string& file) {
        std::ifstream is(file);
        cereal::JSONInputArchive archive(is);
        read_or_write(archive);
    }

} settings;

int main(int argc, char** argv) {
	auto context = Raekor::PlatformContext();

    settings.read_from_disk("config.json");

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
    for(int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        displays.push_back(SDL_Rect());
        SDL_GetDisplayBounds(i, &displays.back());
    }
    // if our display setting is higher than the nr of displays we pick the default display
    auto index = settings.display > displays.size() - 1 ? 0 : settings.display;
    auto main_window = SDL_CreateWindow(settings.name.c_str(),
                                        displays[index].x,
                                        displays[index].y,
                                        displays[index].w,
                                        displays[index].h,
                                        wflags);

	auto directxwindow = SDL_CreateWindow("directx window", 100, 100, 1280, 720, d3wflags);

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

    SDL_GLContext gl_context = SDL_GL_CreateContext(main_window);
    SDL_GL_MakeCurrent(main_window, gl_context);

    SDL_SetWindowResizable(main_window, SDL_FALSE);

    m_assert(gl3wInit() == 0, "failed to init gl3w");

    std::cout << "GL INFO: OpenGL " << glGetString(GL_VERSION);
    printf("GL INFO: OpenGL %s, GLSL %s\n", glGetString(GL_VERSION),
            glGetString(GL_SHADING_LANGUAGE_VERSION));

    std::vector<IDXGIAdapter*> GPUs;
    IDXGIFactory* gpu_factory = NULL;
    IDXGIAdapter* gpu;
    CreateDXGIFactory1(__uuidof(IDXGIFactory), (void**)&gpu_factory);
    for(unsigned int i = 0; gpu_factory->EnumAdapters(i, &gpu) != DXGI_ERROR_NOT_FOUND; i++) {
        GPUs.push_back(gpu);
    }

    for(IDXGIAdapter* adapter : GPUs) {
        DXGI_ADAPTER_DESC description;
        adapter->GetDesc(&description);
        std::wcout << description.Description << std::endl;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImFont* pFont = io.Fonts->AddFontFromFileTTF(settings.font.c_str(), 18.0f);
    if(!io.Fonts->Fonts.empty()) {
        io.FontDefault = io.Fonts->Fonts.back();
    }
    ImGui::StyleColorsDark();

    auto vendor = glGetString(GL_VENDOR);
    auto renderer = glGetString(GL_RENDERER);
    std::cout << vendor << " : " << renderer << std::endl;
    
    ImGui_ImplSDL2_InitForOpenGL(main_window, &gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    //glEnable(GL_CULL_FACE); //uncomment this for model geometry culling

    glClearColor(0.15f, 0.25f, 0.35f, 1.0f);

    GLuint vertex_array_id;
    glGenVertexArrays(1, &vertex_array_id);
    glBindVertexArray(vertex_array_id);

	auto simple_shader = Raekor::GLShader("shaders/simple.vert", "shaders/simple.frag");
	auto skybox_shader = Raekor::GLShader("shaders/skybox.vert", "shaders/skybox.frag");
	auto simple_shader_mvp = simple_shader.get_uniform_location("MVP");
	auto skybox_shader_mvp = skybox_shader.get_uniform_location("MVP");

    Raekor::Camera camera(glm::vec3(0, 0, 5), 45.0f);

    // Setup our scene
    std::vector<Raekor::TexturedModel> scene;
    int active_model = 0;

    // persistent imgui variable values
    std::string mesh_file = "";
    std::string texture_file = "";
    float last_input_scale = 1.0f;
    glm::vec3 last_pos(0.0f);
	auto active_skybox = settings.skyboxes.begin();

    // frame buffer for the renderer
    std::unique_ptr<Raekor::FrameBuffer> frame_buffer;
    frame_buffer.reset(Raekor::FrameBuffer::construct(glm::vec2(1280, 720)));

	std::unique_ptr<Raekor::GLTextureCube> skybox_texture;
	skybox_texture.reset(new Raekor::GLTextureCube(active_skybox->second));
	std::unique_ptr<Raekor::Mesh> skycube;
	skycube.reset(new Raekor::Mesh("resources/models/testcube.obj", Raekor::Mesh::file_format::OBJ));

    //main application loop
    for(;;) {
        //handle sdl and imgui events
		handle_sdl_gui_events({ main_window, directxwindow }, camera);

		const float clear_color[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
		d3context->ClearRenderTargetView(d3_backbuffer, clear_color);
		swap_chain->Present(0, 0);

        glClearColor(0.22f, 0.32f, 0.42f, 1.0f);        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto fsize = frame_buffer->get_size();
        glViewport(0, 0, (GLsizei)fsize.x, (GLsizei)fsize.y);
        frame_buffer->bind();
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDepthMask(GL_FALSE);
		skybox_shader.bind();
        camera.update();
		skybox_shader.upload_uniform_matrix4fv(skybox_shader.get_uniform_location("MVP"), &camera.get_mvp()[0][0]);
		skybox_texture->bind();
        skycube->render();
		skybox_texture->unbind();
        glDepthMask(GL_TRUE);

		simple_shader.bind();
        for(auto& m : scene) {
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

        if(ImGui::BeginMainMenuBar()) {
            if(ImGui::BeginMenu("File")) {
                if(ImGui::MenuItem("Close", "Esc")) {
                    //clean up imgui
                    ImGui_ImplOpenGL3_Shutdown();
                    ImGui_ImplSDL2_Shutdown();
                    ImGui::DestroyContext();

                    //clean up SDL
                    SDL_GL_DeleteContext(gl_context);
                    SDL_DestroyWindow(main_window);
                    SDL_Quit();
                    return 0;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // renderer viewport
        ImGui::SetNextWindowContentSize(ImVec2(fsize.x, fsize.y));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
        ImGui::Begin("Renderer", NULL, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Image((void*)static_cast<size_t>(frame_buffer->get_frame()), ImVec2(fsize.x, fsize.y), {0,1}, {1,0});
        ImGui::End();
        ImGui::PopStyleVar();
        
        // scene panel
        ImGui::SetNextWindowSize(ImVec2(760, 260), ImGuiCond_Once);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 770, io.DisplaySize.y - 550), ImGuiCond_Once);
        ImGui::Begin("Scene");
		if (ImGui::BeginCombo("Skybox", active_skybox->first.c_str())) {
			for (auto it = settings.skyboxes.begin(); it != settings.skyboxes.end(); it++) {
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
                    } else {
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
        ImGui::SameLine();
        if (ImGui::Button("+")) {
            std::string path = context.open_file_dialog({".obj"});
            if(!path.empty()) {
                scene.push_back(Raekor::TexturedModel(path, ""));
                active_model = (int)scene.size() - 1;
                mesh_file = Raekor::get_extension(scene[active_model].get_mesh()->mesh_path);
                texture_file = "";
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("-") && !scene.empty()) {
            scene.erase(scene.begin() + active_model);
            active_model = 0;

            // if we just deleted the last model, empty the strings
            if (scene.empty()) {
                mesh_file = "";
                texture_file = "";
            } else {
                mesh_file = Raekor::get_extension(scene[active_model].get_mesh()->mesh_path);
                if(scene[active_model].get_texture() != nullptr) {
                    texture_file = Raekor::get_extension(scene[active_model].get_texture()->get_path());
                } else {
                    texture_file = "";
                }

            }
        }
        static bool is_vsync = true;
        if(ImGui::RadioButton("USE VSYNC", is_vsync)) {
            is_vsync = !is_vsync;
            SDL_GL_SetSwapInterval(is_vsync);
        }
        ImGui::End();
        
        if(!scene.empty()) {
            //start drawing a new imgui window. TODO: make this into a reusable component
            ImGui::SetNextWindowSize(ImVec2(760, 260), ImGuiCond_Once);
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 770, io.DisplaySize.y - 270), ImGuiCond_Once);
            ImGui::Begin("Object Properties");

            ImGui::Text("Mesh");
            ImGui::Text(mesh_file.c_str(), NULL);
            ImGui::SameLine();
            if (ImGui::Button("...##Mesh")) {
                std::string path = context.open_file_dialog({".obj"});
                if (!path.empty()) {
                    scene[active_model].set_mesh(path);
                    mesh_file = Raekor::get_extension(scene[active_model].get_mesh()->mesh_path);

                }
            }
            ImGui::Separator();

            ImGui::Text("Texture");
            ImGui::Text(texture_file.c_str(), NULL);
            ImGui::SameLine();
            if (ImGui::Button("...##Texture")) {
                std::string path = context.open_file_dialog({".png"});
                if (!path.empty()) {
                    scene[active_model].set_texture(path);
                    texture_file = Raekor::get_extension(scene[active_model].get_texture()->get_path());
                }
            }

            if (ImGui::InputFloat("Scale", &scene[active_model].get_mesh()->scale, 0.05f, 0.1f, "%.2f")) {
                glm::vec3 factor(scene[active_model].get_mesh()->scale / last_input_scale);
                scene[active_model].get_mesh()->scale_by(factor);
                last_input_scale = scene[active_model].get_mesh()->scale;
            }

            // rotation is continuous for now so we have something to animate
            if (ImGui::SliderFloat3("Rotate", &scene[active_model].get_mesh()->euler_rotation.x, -0.10f, 0.10f, "%.3f")) {}

            if (ImGui::InputFloat3("Move", &scene[active_model].get_mesh()->position.x, 2)) {
                glm::vec3 delta = scene[active_model].get_mesh()->position - last_pos;
                scene[active_model].get_mesh()->move(delta);
                last_pos = scene[active_model].get_mesh()->position;
            }

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
	swap_chain->Release();
	d3device->Release();
	d3context->Release();
	d3_backbuffer->Release();
    return 0;
}