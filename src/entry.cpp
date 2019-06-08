#include "pch.h"
#include "entry.h"
#include "math.h"
#include "gl_shader.h"
#include "util.h"
#include "dds.h"
#include "model.h"


#ifdef _WIN32
std::string OpenFile() {
    OPENFILENAMEA ofn;
    CHAR szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "All\0*.*\0";
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
#else
std::string OpenFile() {
    //init gtk
	assert(gtk_init_check(NULL, NULL));
	
    // get action handle and set a title and ok text
	const char* title = "Open File";
	const char* ok_text = "Open";

    // allocate a new dialog window
	GtkWidget *dialog = gtk_file_chooser_dialog_new(
		title,
		NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		ok_text, GTK_RESPONSE_ACCEPT,
		NULL);

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

int main(int argc, char** argv) {
    json config;
    std::ifstream ifs("config.json");
    ifs >> config;	

    if(SDL_Init(SDL_INIT_VIDEO) != 0) {
        trace("failed to init sdl");
    }
    
    auto resolution = jfind<json>(config, "resolution");
    const char* glsl_version = "#version 330";

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);	

    Uint32 wflags = SDL_WINDOW_OPENGL | 
                    SDL_WINDOW_RESIZABLE | 
                    SDL_WINDOW_ALLOW_HIGHDPI;

    auto main_window = SDL_CreateWindow(jfind<std::string>(config, "name").c_str(),
                                        25, 25,
                                        jfind<int>(resolution, "width"),
                                        jfind<int>(resolution, "height"),
                                        wflags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(main_window);
    SDL_GL_MakeCurrent(main_window, gl_context);

    if (gl3wInit() != 0) {
        trace("failed to init gl3w");
    }
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

    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);

    GLuint vertex_array_id;
    glGenVertexArrays(1, &vertex_array_id);
    glBindVertexArray(vertex_array_id);

    unsigned int programID = load_shaders("shaders/vertex.glsl", "shaders/fragment.glsl");
    unsigned int matrixID = glGetUniformLocation(programID, "MVP");
    unsigned int view_matrixID = glGetUniformLocation(programID, "V");
    unsigned int model_matrixID = glGetUniformLocation(programID, "M");

    GE_camera camera;

    // fov , ratio, display range
    camera.projection = glm::perspective(glm::radians(90.0f), 16.0f / 9.0f, 0.1f, 100.0f);

    // Camera matrix
    camera.view = glm::lookAt(
        glm::vec3(0, 0, 5), // camera pos in world space
        glm::vec3(0, 0, 0), // camera looks at the origin
        glm::vec3(0, 1, 0)  // Head is up (0,-1,0 to look upside-down)
    );

    //create the model
    GE_model m;
    GE_model m2;

    //ask for and get the model's properties from the config file
    auto models = jfind<json>(config, "objects");

    std::vector<std::string> object_names;
    for(auto & model : models.items()) {
        object_names.push_back(model.key());
    }
    const char * current_model = object_names.begin()->c_str();

    // get our json object, get it's iterator in the names vector, then get the index from the iterator. 
    // We use the index as a handle for the ImGui drop down list
    auto chosen_object = jfind<json>(models, current_model);
    auto model_it = std::find(object_names.begin(), object_names.end(), object_names.begin()->c_str());
    int model_index = static_cast<int>(std::distance( object_names.begin(), model_it));

    auto texture  = BMP_to_GL(jfind<std::string>(chosen_object, "texture").c_str());
    auto sampler_id = glGetUniformLocation(programID, "myTextureSampler");

    // Read the .obj file
    auto filename = jfind<std::string>(chosen_object, "model");
    std::cout << "loading " << filename << "..." << std::endl;
    if (!GE_load_obj(filename.c_str(), m.vertices, m.uvs, m.normals)) {
        trace("Could not load object");
    }
    if (!GE_load_obj(filename.c_str(), m2.vertices, m2.uvs, m2.normals)) {
        trace("Could not load object");
    }

    //index the vbo for improved performance
    index_model_vbo(m);
    index_model_vbo(m2);

    //generate opengl buffers
    auto vertexbuffer = gen_gl_buffer(m.vertices, GL_ARRAY_BUFFER);
    auto uvbuffer = gen_gl_buffer(m.uvs, GL_ARRAY_BUFFER);
    auto elementbuffer = gen_gl_buffer(m.indices, GL_ELEMENT_ARRAY_BUFFER);
    auto vertexbuffer2 = gen_gl_buffer(m.vertices, GL_ARRAY_BUFFER);
    auto uvbuffer2 = gen_gl_buffer(m.uvs, GL_ARRAY_BUFFER);
    auto elementbuffer2 = gen_gl_buffer(m.indices, GL_ELEMENT_ARRAY_BUFFER);


    //get a rotation matrix
    vec3 euler_rotation(0.0f, 0.0f, 0.0f);
    auto rotation = static_cast<quat>( euler_rotation );
    mat4 rotation_matrix = glm::toMat4(rotation);

    glm::mat4 mvp;
    glUseProgram(programID);



    //main application loop
    for(;;) {
        //handle sdl and imgui events
        handle_sdl_gui_events(main_window, camera.mouse_active);

        //if mouse is not active it means we're in free mode
        if (!camera.mouse_active) {
            calculate_view(main_window, camera);
        }
        
        //get new frame for opengl, sdl and imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(main_window);
        ImGui::NewFrame();

        //start drawing a new imgui window. TODO: make this into a reusable component
        ImGui::Begin("Object Properties");

        ImGui::Text("Mesh");
        ImGui::SameLine();
		if (ImGui::Button("...##Mesh")) {
            std::string path = OpenFile();
			if (path != "") {
                std::string extension = "";
                extension.append(path, path.length()-4, 4);
                if(extension == ".obj") {
                    m = GE_model();
                    GE_load_obj(path.c_str(), m.vertices, m.uvs, m.normals);
                    index_model_vbo(m);
                    vertexbuffer = gen_gl_buffer(m.vertices, GL_ARRAY_BUFFER);
                    uvbuffer = gen_gl_buffer(m.uvs, GL_ARRAY_BUFFER);
                    elementbuffer = gen_gl_buffer(m.indices, GL_ELEMENT_ARRAY_BUFFER);
                }
			}
        }
        ImGui::Separator();

        ImGui::Text("Texture");
        ImGui::SameLine();
		if (ImGui::Button("...##Texture")) {
            std::string path = OpenFile();
			if (path != "") {
                std::string extension = "";
                extension.append(path, path.length()-4, 4);
                if(extension == ".bmp") {
                    texture = BMP_to_GL(path.c_str());
                }
			}
        }

        if(ImGui::BeginCombo("models", current_model)) {
            for(auto & name : object_names) {
                bool selected = (name == current_model);
                if(ImGui::Selectable(name.c_str(), selected)) {
                    current_model = name.c_str();
                    m = GE_model();
                    chosen_object = jfind<json>(models, current_model);

                    auto model_path = jfind<std::string>(chosen_object, "model");
                    auto texture_path = jfind<std::string>(chosen_object, "texture");

                    GE_load_obj(model_path.c_str(), m.vertices, m.uvs, m.normals);
                    texture  = BMP_to_GL(jfind<std::string>(chosen_object, "texture").c_str());
                    index_model_vbo(m);

                    vertexbuffer = gen_gl_buffer(m.vertices, GL_ARRAY_BUFFER);
                    uvbuffer = gen_gl_buffer(m.uvs, GL_ARRAY_BUFFER);
                    elementbuffer = gen_gl_buffer(m.indices, GL_ELEMENT_ARRAY_BUFFER);
                }
                if(selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }	
            ImGui::EndCombo();
        }

        static float last_input_scale = 1.0f;
        static float input_scale = 1.0f;
        if (ImGui::SliderFloat("Scale", &input_scale, 0.01f, 2.0f, "%.2f")) {
            vec3 factor(input_scale / last_input_scale);
            m.transformation = scale(m.transformation, factor);
            last_input_scale = input_scale;
        }

        if (ImGui::SliderFloat3("Rotate", &euler_rotation.x, -0.10f, 0.10f, "%.2f")) {
            rotation = static_cast<quat>(euler_rotation);
            rotation_matrix = glm::toMat4(rotation);
        }

        static vec3 model_pos(0.0f);
        static vec3 pos(0.0f);
        if (ImGui::SliderFloat3("Move", &pos.x, -4.0f, 4.0f, "%.2f")) {
            vec3 move = pos - model_pos;
            m.transformation = translate(m.transformation, move);
            model_pos = pos;
        }

        if (ImGui::Button("Reset")) {
            last_input_scale = 1.0f;
            input_scale = 1.0f;
            euler_rotation = vec3(0.0f);
            rotation = static_cast<quat>(euler_rotation);
            rotation_matrix = glm::toMat4(rotation);
            model_pos = vec3(0.0f);
            pos = vec3(0.0f);
            m.transformation = mat4(1.0f);
        }

        ImGui::End();
        ImGui::Render();

        //set the viewport
        int w, h;
        SDL_GetWindowSize(main_window, &w, &h);
        glViewport(0, 0, w, h);

        //clear frame, use our shaders and perform mvp transformation
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(programID);
        
        //rotate the model and rebuild our mvp
        m.transformation = m.transformation * rotation_matrix;
        mvp = camera.projection * camera.view * m.transformation;

        glUniformMatrix4fv(matrixID, 1, GL_FALSE, &mvp[0][0]);

        //bind the model texture to be the first
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(sampler_id, 0);

        //set attribute buffer for model vertices
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0,  (void*)0 );

        //set attribute buffer for uvs
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

        // Draw triangles
        glDrawElements(GL_TRIANGLES, static_cast<int>(m.indices.size()), GL_UNSIGNED_INT, nullptr);

        mvp = camera.projection * camera.view * m2.transformation;
        glUniformMatrix4fv(matrixID, 1, GL_FALSE, &mvp[0][0]);

        //set attribute buffer for model vertices
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer2);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0,  (void*)0 );

        //set attribute buffer for uvs
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, uvbuffer2);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

        // Draw triangles
        glDrawElements(GL_TRIANGLES, static_cast<int>(m2.indices.size()), GL_UNSIGNED_INT, nullptr);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);

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