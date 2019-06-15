#include "pch.h"
#include "entry.h"
#include "math.h"
#include "gl_shader.h"
#include "util.h"
#include "dds.h"
#include "camera.h"
#include "mesh.h"

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
        gtk_file_filter_add_pattern (gtk_filter, filter.c_str());
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

std::string get_filename(const std::string& path) {
    std::string filename = "";
    for(size_t i = path.size()-1; i > 0; i--) {
        if (path[i] == '\\' || path[i] == '/') {
            return filename;
        } 
        filename.insert(0, std::string(1, path[i]));
    }
    return std::string();
}

int main(int argc, char** argv) {
    json config;
    std::ifstream ifs("config.json");
    ifs >> config;	

    m_assert(SDL_Init(SDL_INIT_VIDEO) == 0, "failed to init sdl");
    
    auto resolution = jfind<json>(config, "resolution");
    const char* glsl_version = "#version 330";

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);	

    Uint32 wflags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | 
                    SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED;

    auto main_window = SDL_CreateWindow(jfind<std::string>(config, "name").c_str(),
                                        25, 25,
                                        jfind<int>(resolution, "width"),
                                        jfind<int>(resolution, "height"),
                                        wflags);

    SDL_GLContext gl_context = SDL_GL_CreateContext(main_window);
    SDL_GL_MakeCurrent(main_window, gl_context);

    m_assert(gl3wInit() == 0, "failed to init gl3w");

    std::cout << "GL INFO: OpenGL " << glGetString(GL_VERSION);
    printf("GL INFO: OpenGL %s, GLSL %s\n", glGetString(GL_VERSION),
            glGetString(GL_SHADING_LANGUAGE_VERSION));


    auto imgui_properties = jfind<json>(config, "imgui");
    auto font = jfind<std::string>(imgui_properties, "font");
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImFont* pFont = io.Fonts->AddFontFromFileTTF(font.c_str(), 18.0f);
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

    glClearColor(0.03f, 0.1f, 0.1f, 1.0f);

    GLuint vertex_array_id;
    glGenVertexArrays(1, &vertex_array_id);
    glBindVertexArray(vertex_array_id);

    unsigned int programID = load_shaders("shaders/vertex.glsl", "shaders/fragment.glsl");
    unsigned int matrixID = glGetUniformLocation(programID, "MVP");
    unsigned int view_matrixID = glGetUniformLocation(programID, "V");
    unsigned int model_matrixID = glGetUniformLocation(programID, "M");

    Raekor::Camera camera(glm::vec3(0, 0, 5), 45.0f);

    // fov , ratio, display range
    camera.projection = glm::perspective(glm::radians(90.0f), 16.0f / 9.0f, 0.1f, 100.0f);

    // Camera matrix
    camera.view = glm::lookAt(
        glm::vec3(0, 0, 5), // camera pos in world space
        glm::vec3(0, 0, 0), // camera looks at the origin
        glm::vec3(0, 1, 0)  // Head is up (0,-1,0 to look upside-down)
    );

    // texture sampler 
    auto sampler_id = glGetUniformLocation(programID, "myTextureSampler");

    // Setup our scene
    std::vector<Raekor::TexturedMesh> scene;
    int active_model = 0;

    glm::mat4 mvp;
    glUseProgram(programID);

	// persistent imgui variable values
    std::string mesh_file = "";
    std::string texture_file = "";
    float last_input_scale = 1.0f;
    glm::vec3 last_pos(0.0f);

    //main application loop
    for (;;) {
        //handle sdl and imgui events
        handle_sdl_gui_events(main_window, camera);
        
        //get new frame for opengl, sdl and imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(main_window);
        ImGui::NewFrame();

        ImGui::Begin("Scene");
        if (ImGui::BeginCombo("", mesh_file.c_str())) {
            for (int i = 0; i < scene.size(); i++) {
                bool selected = (i == active_model);
                
				//hacky way to give the selectable a unique ID
                auto name = scene[i].mesh_path;
                name = name + "##" + std::to_string(i);
                
				if (ImGui::Selectable(name.c_str(), selected)) {
                    active_model = i;
                    mesh_file = get_filename(scene[active_model].mesh_path);
                    last_input_scale = scene[active_model].scale;
                    last_pos = scene[active_model].position;
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
                scene.push_back(Raekor::TexturedMesh(path, Raekor::Mesh::file_format::OBJ));
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("-")) {
            scene.erase(scene.begin() + active_model);
            active_model = 0;
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
                    scene[active_model].parse_OBJ(path);
                    mesh_file = get_filename(scene[active_model].mesh_path);

                }
            }
            ImGui::Separator();

            ImGui::Text("Texture");
            ImGui::Text(texture_file.c_str(), NULL);
            ImGui::SameLine();
            if (ImGui::Button("...##Texture")) {
                std::string path = OpenFile({".bmp"});
                if (!path.empty()) {
                    scene[active_model].load_texture(path);
                    texture_file = get_filename(scene[active_model].image_path);
                }
            }

            if (ImGui::InputFloat("Scale", &scene[active_model].scale, 0.05f, 0.1f, "%.2f")) {
                glm::vec3 factor(scene[active_model].scale / last_input_scale);
                scene[active_model].scale_by(factor);
                last_input_scale = scene[active_model].scale;
            }

            // rotation is continuous for now so we have something to animate
            if (ImGui::SliderFloat3("Rotate", &scene[active_model].euler_rotation.x, -0.10f, 0.10f, "%.3f")) {}

            if (ImGui::InputFloat3("Move", &scene[active_model].position.x, 2)) {
                glm::vec3 delta = scene[active_model].position - last_pos;
                scene[active_model].move(delta);
                last_pos = scene[active_model].position;
            }

            if (ImGui::Button("Reset")) {
                last_input_scale = 1.0f;
                last_pos = glm::vec3(0.0f);
                scene[active_model].reset_transformation();
            }
        }

        ImGui::End();
        ImGui::Render();

        int w, h;
        SDL_GetWindowSize(main_window, &w, &h);
        glViewport(0, 0, w, h);

        //clear frame, use our shaders and perform mvp transformation
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(programID);

        for(auto m = scene.begin(); m != scene.end(); m++) {
            m->rotate(m->get_rotation_matrix());
            mvp = camera.projection * camera.view * m->get_transformation();
            glUniformMatrix4fv(matrixID, 1, GL_FALSE, &mvp[0][0]);
            m->render(sampler_id);
        }

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(main_window);
    }

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