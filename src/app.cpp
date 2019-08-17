#include "pch.h"
#include "app.h"
#include "util.h"
#include "model.h"
#include "entry.h"
#include "camera.h"
#include "shader.h"
#include "framebuffer.h"
#include "PlatformContext.h"
#include "renderer.h"
#include "buffer.h"
#include "DXResourceBuffer.h"
#include "scene.h"

namespace Raekor {

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

void Application::run() {
    auto context = Raekor::PlatformContext();

    // retrieve the application settings from the config file
    serialize_settings("config.json");

    m_assert(SDL_Init(SDL_INIT_VIDEO) == 0, "failed to init sdl");

    Uint32 wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL |
        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED;

    std::vector<SDL_Rect> displays;
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        displays.push_back(SDL_Rect());
        SDL_GetDisplayBounds(i, &displays.back());
    }

    // if our display setting is higher than the nr of displays we pick the default display
    auto index = display > displays.size() - 1 ? 0 : display;
    auto directxwindow = SDL_CreateWindow(name.c_str(),
        displays[index].x,
        displays[index].y,
        displays[index].w,
        displays[index].h,
        wflags);

    // create the renderer object that does sets up the API and does all the runtime magic
    std::unique_ptr<Renderer> dxr;
    dxr.reset(Renderer::construct(directxwindow));

    // create a Camera we can use to move around our scene
    Camera camera(glm::vec3(0, 0, 5), 45.0f);

    std::unique_ptr<Model> model;
    std::unique_ptr<Shader> dx_shader;
    std::unique_ptr<FrameBuffer> dxfb;
    std::unique_ptr<ResourceBuffer<cb_vs>> dxrb;
    std::unique_ptr<Texture> sky_image;
    std::unique_ptr<Mesh> skycube;
    std::unique_ptr<Shader> sky_shader;

    model.reset(new Model("resources/models/testcube.obj", "resources/textures/test.png"));
    dx_shader.reset(Shader::construct("shaders/simple_vertex", "shaders/simple_fp"));
    dxfb.reset(FrameBuffer::construct({ 1280, 720 }));
    dxrb.reset(ResourceBuffer<cb_vs>::construct());
    sky_image.reset(Texture::construct(skyboxes["lake"]));
    skycube.reset(new Mesh("resources/models/testcube.obj"));
    sky_shader.reset(Shader::construct("shaders/skybox_vertex", "shaders/skybox_fp"));

    Scene scene;
    Scene::iterator active_model = scene.end();

    // persistent imgui variable values
    std::string mesh_file = "";
    std::string texture_file = "";
    auto active_skybox = skyboxes.begin();
    auto fsize = dxfb->get_size();

    SDL_SetWindowInputFocus(directxwindow);

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingWithShift = true;
    ImFont* pFont = io.Fonts->AddFontFromFileTTF(font.c_str(), 18.0f);
    if (!io.Fonts->Fonts.empty()) {
        io.FontDefault = io.Fonts->Fonts.back();
    }

    bool running = true;
    bool transpose = Renderer::get_activeAPI() == RenderAPI::DIRECTX11 ? true : false;

    //main application loop
    while(running) {

        //handle sdl and imgui events
        handle_sdl_gui_events({ directxwindow }, camera);

        dxr->Clear({ 0.22f, 0.32f, 0.42f, 1.0f });
        dxfb->bind();
        dxr->Clear({ 0.0f, 0.32f, 0.42f, 1.0f });

        // bind the shader
        sky_shader->bind();
        // update the camera without translation/model matrix
        camera.update();
        // upload the camera's mvp matrix
        dxrb->get_data().MVP = camera.get_mvp(transpose);
        // bind the resource buffer containing the mvp
        dxrb->bind(0);
        // bind the skybox cubemap
        sky_image->bind();
        // bind the skycube mesh
        skycube->bind();
        // draw the indexed vertices
        dxr->DrawIndexed(skycube->get_index_buffer()->get_count(), false);

        // bind the model's shader
        dx_shader->bind();
        // add rotation to the cube so somethings animated
        auto cube_rotation = model->get_mesh()->rotation_ptr();
        *cube_rotation += 0.01f;
        for (auto& m : scene) {
            Model& model = m.second;
            // if the model does not have a mesh set, continue
            if (!model.get_mesh()) continue;
            model.get_mesh()->recalc_transform();
            camera.update(model.get_mesh()->get_transform());
            dxrb->get_data().MVP = camera.get_mvp(transpose);
            dxrb->bind(0);
            model.bind();
            dxr->DrawIndexed(model.get_mesh()->get_index_buffer()->get_count());
        }

        // unbind the framebuffer which switches to application's backbuffer
        dxfb->unbind();

        //get new frame for render API, sdl and imgui
        dxr->ImGui_NewFrame(directxwindow);

        static bool opt_fullscreen_persistant = true;
        bool opt_fullscreen = opt_fullscreen_persistant;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen) {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }

        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the pass-thru hole, so we ask Begin() to not render a background.
        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        static bool p_open = true;
        ImGui::Begin("DockSpace", &p_open, window_flags);
        ImGui::PopStyleVar();
        if (opt_fullscreen) ImGui::PopStyleVar(2);

        // DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        // draw the top level bar that contains stuff like "File" and "Edit"
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Close", "CTRL+S")) {
                    //clean up SDL
                    SDL_DestroyWindow(directxwindow);
                    SDL_Quit();
                    exit(0);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // renderer viewport
        ImGui::SetNextWindowContentSize(ImVec2(fsize.x, fsize.y));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Renderer", NULL, ImGuiWindowFlags_AlwaysAutoResize);

        auto image_data = dxfb->ImGui_data();
        if (Renderer::get_activeAPI() == RenderAPI::DIRECTX11) {
            ImGui::Image((ID3D11ShaderResourceView*)image_data, ImVec2(fsize.x, fsize.y));
        }
        else if (Renderer::get_activeAPI() == RenderAPI::OPENGL) {
            ImGui::Image(image_data, ImVec2(fsize.x, fsize.y), { 0,1 }, { 1,0 });
        }
        ImGui::End();
        ImGui::PopStyleVar();

        dxfb->unbind();

        // model panel
        ImGui::Begin("ECS");
        auto tree_node_flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick 
            | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;
        if (ImGui::TreeNodeEx("Models", tree_node_flags)) {
            ImGui::Columns(1, NULL, false);
            for (Scene::iterator it = scene.begin(); it != scene.end(); it++) {
                bool selected = it == active_model;
                std::string label = it->first;
                if (ImGui::Selectable(label.c_str(), selected)) {
                    active_model = it;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::TreePop();
        }

        //ImGui::SetKeyboardFocusHere();
        static char input_text[120];
        if (ImGui::InputText("", input_text, sizeof(input_text))) {
        }
        ImGui::SameLine();
        if (ImGui::Button("Add")) {
            std::string model_name = std::string(input_text);
            if (!model_name.empty()) {
                scene.add(model_name);
                active_model = scene[model_name.c_str()];
                memset(input_text, 0, sizeof(input_text));
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove")) {
            if (active_model != scene.end()) {
                scene.remove(active_model->first);
                active_model = scene.end();
            }
        }
        if (ImGui::Button("Load Mesh")) {
            if (active_model != scene.end()) {
                std::string path = context.open_file_dialog({ ".obj", ".fbx" });
                if (!path.empty()) {
                    active_model->second.set_mesh(path);
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Load texture")) {
            if (active_model != scene.end()) {
                std::string path = context.open_file_dialog({ ".png", ".jpg", ".bmp", ".tga" });
                if (!path.empty()) {
                    active_model->second.set_texture(path);
                }
            }
        }
        ImGui::End();

        // scene panel
        ImGui::SetNextWindowSize(ImVec2(760, 260), ImGuiCond_Once);
        ImGui::Begin("Scene");
        if (ImGui::BeginCombo("Skybox", active_skybox->first.c_str())) {
            for (auto it = skyboxes.begin(); it != skyboxes.end(); it++) {
                bool selected = (it == active_skybox);
                if (ImGui::Selectable(it->first.c_str(), selected)) {
                    active_skybox = it;
                    sky_image.reset(Texture::construct(active_skybox->second));
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::RadioButton("OpenGL", Renderer::get_activeAPI() == RenderAPI::OPENGL)) {
            // check if the active api is not already openGL
            if (Renderer::get_activeAPI() != RenderAPI::OPENGL) {
                Renderer::set_activeAPI(RenderAPI::OPENGL);
                running = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("DirectX 11", Renderer::get_activeAPI() == RenderAPI::DIRECTX11)) {
            // check if the active api is not already directx
            if (Renderer::get_activeAPI() != RenderAPI::DIRECTX11) {
                Renderer::set_activeAPI(RenderAPI::DIRECTX11);
                running = false;
            }
        }

        // toggle button for openGl vsync
        static bool is_vsync = true;
        if (ImGui::RadioButton("USE VSYNC", is_vsync)) {
            is_vsync = !is_vsync;
            SDL_GL_SetSwapInterval(is_vsync);
        }
        ImGui::End();

        // if the scene containt at least one model, AND the active model is pointing at a valid model,
        // AND the active model has a mesh to modify, the properties window draws
        if (!scene.empty() && active_model != scene.end() && active_model->second) {
            ImGui::SetNextWindowSize(ImVec2(760, 260), ImGuiCond_Once);
            ImGui::Begin("Model Properties");

            if (ImGui::DragFloat3("Scale", active_model->second.get_mesh()->scale_ptr(), 0.01f, 0.0f, 10.0f)) {
            }
            if (ImGui::DragFloat3("Position", active_model->second.get_mesh()->pos_ptr(), 0.01f, -100.0f, 100.0f)) {
            }
            if (ImGui::DragFloat3("Rotation", active_model->second.get_mesh()->rotation_ptr(), 0.01f, (float)(-M_PI), (float)(M_PI))) {
            }

            // resets the model's transformation
            if (ImGui::Button("Reset")) {
                active_model->second.get_mesh()->reset_transform();
            }
            ImGui::End();
        }
        ImGui::End();
            dxr->ImGui_Render();
            dxr->SwapBuffers();
        }
    }

} // namespace Raekor