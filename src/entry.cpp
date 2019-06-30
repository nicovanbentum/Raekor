#include "pch.h"
#include "entry.h"
#include "math.h"
#include "gl_shader.h"
#include "util.h"
#include "camera.h"
#include "mesh.h"
#include "model.h"
#include "framebuffer.h"

#ifdef _WIN32
std::string OpenFile(const std::vector<std::string>& filters) {
    
    std::string lpstr_filters;
    for(auto& f : filters) {
        lpstr_filters.append(std::string(f + '\0' + '*' + f + '\0'));
    }

    OPENFILENAMEA ofn;
    CHAR szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = lpstr_filters.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return ofn.lpstrFile;
    }
    return std::string();
}
#elif __linux__
std::string OpenFile(const std::vector<std::string>& filters) {
    //init gtk
	m_assert(gtk_init_check(NULL, NULL), "failed to init gtk");	
    // allocate a new dialog window
	auto dialog = gtk_file_chooser_dialog_new(
		"Open File",
		NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		"Open", GTK_RESPONSE_ACCEPT,
		NULL);

    for(auto& filter : filters) {
        auto gtk_filter = gtk_file_filter_new();
        gtk_file_filter_set_name(gtk_filter, filter.c_str());
        gtk_file_filter_add_pattern (gtk_filter, std::string("*" + filter).c_str());
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), gtk_filter);
    }

	char* path = NULL;
    // if the ok button is pressed (gtk response type ACCEPT) we get the selected filepath
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	}
    // destroy our dialog window
	gtk_widget_destroy(dialog);
    // if our filepath is not empty we make it the return value
	std::string file;
	if(path) {
		file = std::string(path, strlen(path));
	}
    // main event loop for our window, this took way too long to fix 
    // (newer GTK's produce segfaults, something to do with SDL)
	while(gtk_events_pending()) {
		gtk_main_iteration();
    }
	return file;
}
#endif

struct settings {
    std::string name;
    std::string font;
    uint8_t display;

    template<class C>
    void read_or_write(C& archive) {
        archive(
            CEREAL_NVP(name), 
            CEREAL_NVP(display), 
            CEREAL_NVP(font));
    }

	void read_from_disk(const std::string& file) {
		std::ifstream is(file);
		cereal::JSONInputArchive archive(is);
		read_or_write(archive);
	}

} settings;

int main(int argc, char** argv) {

	settings.read_from_disk("config.json");

    m_assert(SDL_Init(SDL_INIT_VIDEO) == 0, "failed to init sdl");
    
    const char* glsl_version = "#version 330";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);	

    Uint32 wflags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | 
                    SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED;

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

    SDL_GLContext gl_context = SDL_GL_CreateContext(main_window);
    SDL_GL_MakeCurrent(main_window, gl_context);

    SDL_SetWindowResizable(main_window, SDL_FALSE);

    m_assert(gl3wInit() == 0, "failed to init gl3w");

    std::cout << "GL INFO: OpenGL " << glGetString(GL_VERSION);
    printf("GL INFO: OpenGL %s, GLSL %s\n", glGetString(GL_VERSION),
            glGetString(GL_SHADING_LANGUAGE_VERSION));


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

    unsigned int programID = load_shaders("shaders/vertex.glsl", "shaders/fragment.glsl");
    unsigned int matrixID = glGetUniformLocation(programID, "MVP");

    Raekor::Camera camera(glm::vec3(0, 0, 5), 45.0f);

    // texture sampler 
    auto sampler_id = glGetUniformLocation(programID, "myTextureSampler");

    // Setup our scene
    std::vector<Raekor::TexturedModel> scene;
    int active_model = 0;

	// persistent imgui variable values
    std::string mesh_file = "";
    std::string texture_file = "";
    float last_input_scale = 1.0f;
    glm::vec3 last_pos(0.0f);

    // frame buffer for the renderer
    std::unique_ptr<Raekor::FrameBuffer> frame_buffer;
    frame_buffer.reset(Raekor::FrameBuffer::construct(glm::vec2(1440, 900)));
    
    glUseProgram(programID);
    //main application loop
    for(;;) {
        //handle sdl and imgui events
        handle_sdl_gui_events(main_window, camera);

        glClearColor(0.22f, 0.32f, 0.42f, 1.0f);        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto fsize = frame_buffer->get_size();
        glViewport(0, 0, (GLsizei)fsize.x, (GLsizei)fsize.y);
        frame_buffer->bind();
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for(auto& m : scene) {
            m.get_mesh()->rotate(m.get_mesh()->get_rotation_matrix());
            camera.update(m.get_mesh()->get_transformation());
            glUniformMatrix4fv(matrixID, 1, GL_FALSE, &camera.get_mvp()[0][0]);
            m.render();
        }
        frame_buffer->unbind();
        
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
            }
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
            std::string path = OpenFile({".obj"});
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
            ImGui::SetWindowSize({800,800});
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
                std::string path = OpenFile({".obj"});
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
                std::string path = OpenFile({".bmp"});
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
    return 0;
}