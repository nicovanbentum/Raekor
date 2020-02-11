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
#include "timer.h"

// TODO: sort this out, consider changing the entire resource buffer API
#ifdef _WIN32
#include "DXResourceBuffer.h"
#endif

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

    int sdl_err = SDL_Init(SDL_INIT_VIDEO);
    m_assert(sdl_err == 0, "failed to init SDL for video");

    Uint32 wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL |
        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED;

    std::vector<SDL_Rect> displays;
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        displays.push_back(SDL_Rect());
        SDL_GetDisplayBounds(i, &displays.back());
    }

    // if our display setting is higher than the nr of displays we pick the default display
    display = display > displays.size() - 1 ? 0 : display;
    auto directxwindow = SDL_CreateWindow(name.c_str(),
        displays[display].x,
        displays[display].y,
        displays[display].w,
        displays[display].h,
        wflags);

    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // create the renderer object that does sets up the API and does all the runtime magic
    Renderer::set_activeAPI(RenderAPI::OPENGL);
    Render::Init(directxwindow);

    // create a Camera we can use to move around our scene
    Camera camera(glm::vec3(0, 1.0, 0), 45.0f);

    MVP ubo;

    std::unique_ptr<Model> model;
    std::unique_ptr<Shader> dx_shader;
    std::unique_ptr<FrameBuffer> dxfb;
    std::unique_ptr<ResourceBuffer> dxrb;
    std::unique_ptr<Texture> sky_image;
    std::unique_ptr<Mesh> skycube;
    std::unique_ptr<Shader> sky_shader;

    Ffilter ft_mesh;
    ft_mesh.name = "Supported Mesh Files";
    ft_mesh.extensions = "*.obj;*.fbx;*.gltf";

    Ffilter ft_texture;
    ft_texture.name = "Supported Image Files";
    ft_texture.extensions = "*.png;*.jpg;*.jpeg;*.tga";

    std::vector<Shader::Stage> gl_model_shaders;
    gl_model_shaders.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\simple_vertex.glsl");
    gl_model_shaders.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\simple_fp.glsl");

    std::vector<Shader::Stage> gl_skybox_shaders;
    gl_skybox_shaders.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\skybox_vertex.glsl");
    gl_skybox_shaders.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\skybox_fp.glsl");

    std::vector<Shader::Stage> dx_model_shaders;
    dx_model_shaders.emplace_back(Shader::Type::VERTEX, "shaders\\DirectX\\simple_vertex.cso");
    dx_model_shaders.emplace_back(Shader::Type::FRAG, "shaders\\DirectX\\simple_fp.cso");

    std::vector<Shader::Stage> dx_skybox_shaders;
    dx_skybox_shaders.emplace_back(Shader::Type::VERTEX, "shaders\\DirectX\\skybox_vertex.cso");
    dx_skybox_shaders.emplace_back(Shader::Type::FRAG, "shaders\\DirectX\\skybox_fp.cso");


    dx_shader.reset(Shader::construct(gl_model_shaders.data(), gl_model_shaders.size()));
    sky_shader.reset(Shader::construct(gl_skybox_shaders.data(), gl_skybox_shaders.size()));

    skycube.reset(new Mesh(Shape::Cube));
    skycube->get_vertex_buffer()->set_layout({ 
        {"POSITION", ShaderType::FLOAT3}, 
        {"UV", ShaderType::FLOAT2}, 
        {"NORMAL", ShaderType::FLOAT3} 
    });

    sky_image.reset(Texture::construct(skyboxes["lake"]));
    dxrb.reset(ResourceBuffer::construct(sizeof(MVP)));
    dxfb.reset(FrameBuffer::construct({ displays[display].w * 0.80, displays[display].h * 0.80 }));

    using Scene = std::vector<Model>;
    Scene models;
    Scene::iterator active_m = models.end();

    // persistent imgui variable values
    auto active_skybox = skyboxes.find("lake");

    SDL_SetWindowInputFocus(directxwindow);

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigDockingWithShift = true;
    ImFont* pFont = io.Fonts->AddFontFromFileTTF(font.c_str(), 18.0f);
    if (!io.Fonts->Fonts.empty()) {
        io.FontDefault = io.Fonts->Fonts.back();
    }
    static unsigned int selected_mesh = 0;
    bool show_settings_window = false;

    Timer dt_timer;
    double dt = 0;

    for (const std::string& path : project) {
        models.push_back(Model(path));
        active_m = models.end() - 1;
        auto scale = active_m->scale_ptr();
        active_m->recalc_transform();
    }

    glm::mat4 lightmatrix = glm::mat4(1.0f);
    lightmatrix = glm::translate(lightmatrix, { 0.0, 1.0f, 0.0 });
    float lightPos[3], lightRot[3], lightScale[3];
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(lightmatrix), lightPos, lightRot, lightScale);
    glm::vec4 lightAngle = { 0.0f, 1.0f, 1.0f, 0.0f };
    //main application loop
    while(running) {
        dt_timer.start();
        //handle sdl and imgui events
        handle_sdl_gui_events({ directxwindow }, camera, dt);

        Render::Clear({ 0.22f, 0.32f, 0.42f, 1.0f });
        dxfb->bind();
        Render::Clear({ 0.0f, 0.32f, 0.42f, 1.0f });
        bool transpose = Renderer::get_activeAPI() == RenderAPI::DIRECTX11 ? true : false;

        // bind the shader
        sky_shader->bind();
        // update the camera
        camera.update();
        // upload the camera's mvp matrix
        ubo.model = glm::mat4(1.0f);
        ubo.view = camera.getView();
        ubo.projection = camera.getProjection();
        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(lightmatrix), lightPos, lightRot, lightScale);
        ubo.lightPos = glm::vec4(glm::make_vec3(lightPos), 1.0f);
        ubo.lightAngle = lightAngle;
        // update the resource buffer 
        dxrb->update(&ubo, sizeof(ubo));
        // bind the resource buffer
        dxrb->bind(0);
        // bind the skybox cubemap
        sky_image->bind();
        // bind the skycube mesh
        skycube->bind();
        // draw the indexed vertices
        Render::DrawIndexed(skycube->get_index_buffer()->get_count(), false);

        // bind the model's shader
        dx_shader->bind();
        for (auto& m : models) {
            m.recalc_transform();
            ubo.model = m.get_transform();
            ubo.view = camera.getView();
            dxrb->update(&ubo, sizeof(ubo));
            dxrb->bind(0);
            m.render();
        }

        // unbind the framebuffer which switches to application's backbuffer
        dxfb->unbind();

        //get new frame for render API, sdl and imgui
        Render::ImGui_NewFrame(directxwindow);
        ImGuizmo::BeginFrame();
        ImGuizmo::Enable(true);

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

        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the pass-thru hole, 
        // so we ask Begin() to not render a background.
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

        // move the light by a fixed amount and let it bounce between -125 and 125 units/pixels on the x axis
        static double move_amount = 0.003;
        static double bounds = 10.0f;
        static bool move_light = true;
        double light_delta = move_amount * dt;
        if ((lightPos[0] >= bounds && move_amount > 0) || (lightPos[0] <= -bounds && move_amount < 0)) {
            move_amount *= -1;
        }
        if (move_light) {
            lightmatrix = glm::translate(lightmatrix, { light_delta, 0.0, 0.0 });
        }

        // draw the top level bar that contains stuff like "File" and "Edit"
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save project", "CTRL + S")) {
                    serialize_settings("config.json", true);
                }
                if (ImGui::MenuItem("Settings", "")) {
                    show_settings_window = true;
                }
                if (ImGui::MenuItem("Exit", "Escape")) {
                    running = false;
                }

                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // draw the imguizmo at the center of the light
        ImGuizmo::Manipulate(glm::value_ptr(camera.getView()), glm::value_ptr(camera.getProjection()), ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::MODE::WORLD, glm::value_ptr(lightmatrix));

        // model panel
        ImGui::Begin("ECS");
        auto tree_node_flags = ImGuiTreeNodeFlags_DefaultOpen;
        if (ImGui::TreeNodeEx("Models", tree_node_flags)) {
            ImGui::Columns(1, NULL, false);
            for (Scene::iterator it = models.begin(); it != models.end(); it++) {
                bool selected = it == active_m;
                // draw a tree node for every model in the scene
                ImGuiTreeNodeFlags treeflags = ImGuiTreeNodeFlags_OpenOnDoubleClick;
                if (it == active_m) {
                    treeflags |= ImGuiTreeNodeFlags_Selected;
                }
                auto open = ImGui::TreeNodeEx(it->get_path().c_str(), treeflags);
                if (ImGui::IsItemClicked()) active_m = it;
                if(open) {
                    // draw a selectable for every mesh in the scene
                    for (unsigned int i = 0; i < it->mesh_count(); i++) {
                        ImGui::PushID(i);
                        if (it->get_mesh(i).has_value()) {
                            bool selected = (i == selected_mesh) && (it == active_m);
                            if (ImGui::Selectable(it->get_mesh(i).value()->get_name().c_str(), selected)) {
                                active_m = it;
                                selected_mesh = i;
                            }
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }

        static char input_text[120];
        if (ImGui::InputText("", input_text, sizeof(input_text), ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string model_name = std::string(input_text);
            if (!model_name.empty() && active_m != models.end()) {
                active_m->set_path(model_name);
                memset(input_text, 0, sizeof(input_text));
            } else {
                memset(input_text, 0, sizeof(input_text));
            }
        }
        if (ImGui::Button("Load Model")) {
            std::string path = context.open_file_dialog({ ft_mesh });
            if (!path.empty()) {
                models.push_back(Model(path));
                active_m = models.end() - 1;
                auto scale = active_m->scale_ptr();
                active_m->recalc_transform();
                project.push_back(path);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove Model")) {
            if (active_m != models.end()) {
                models.erase(active_m);
                active_m = models.end();
                int index = (int)std::distance(models.begin(), active_m);
                project.erase(project.begin() + index);
            }
        }
        ImGui::End();

        // scene panel
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

        static bool reset = false;
        if (ImGui::RadioButton("OpenGL", Renderer::get_activeAPI() == RenderAPI::OPENGL)) {
            if(Renderer::set_activeAPI(RenderAPI::OPENGL)) {
                reset = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("DirectX 11", Renderer::get_activeAPI() == RenderAPI::DIRECTX11)) {
            if(Renderer::set_activeAPI(RenderAPI::DIRECTX11)) {
                reset = true;
            }
        }

        // toggle button for openGl vsync
        static bool use_vsync = false;
        if (ImGui::RadioButton("USE VSYNC", use_vsync)) {
            use_vsync = !use_vsync;
        }

        if (ImGui::RadioButton("Animate Light", move_light)) {
            move_light = !move_light;
        }

        if (ImGui::Button("Reload shaders")) {
            switch (Renderer::get_activeAPI()) {
            case RenderAPI::OPENGL: {
                dx_shader.reset(Shader::construct(gl_model_shaders.data(), gl_model_shaders.size()));
                sky_shader.reset(Shader::construct(gl_skybox_shaders.data(), gl_skybox_shaders.size()));
            } break;
            case RenderAPI::DIRECTX11: {
                dx_shader.reset(Shader::construct(dx_model_shaders.data(), dx_model_shaders.size()));
                sky_shader.reset(Shader::construct(dx_skybox_shaders.data(), dx_skybox_shaders.size()));
            }
            }
        }


        ImGui::NewLine(); ImGui::Separator();

        ImGui::Text("Light Properties");
        if (ImGui::DragFloat3("Angle", glm::value_ptr(lightAngle), 0.01f, -1.0f, 1.0f)) {}

        ImGui::End();

        ImGui::ShowMetricsWindow();

        ImGui::Begin("Camera Properties");
        if (ImGui::DragFloat("Camera Move Speed", camera.get_move_speed(), 0.01f, 0.1f, FLT_MAX, "%.2f")) {}
        if (ImGui::DragFloat("Camera Look Speed", camera.get_look_speed(), 0.0001f, 0.0001f, FLT_MAX, "%.4f")) {}
        ImGui::End();

        // if the scene containt at least one model, AND the active model is pointing at a valid model,
        // AND the active model has a mesh to modify, the properties window draws
        if (!models.empty() && active_m != models.end()) {
            ImGui::Begin("Mesh Properties");

            if (ImGui::DragFloat3("Scale", active_m->scale_ptr(), 0.01f, 0.0f, 10.0f)) {}
            if (ImGui::DragFloat3("Position", active_m->pos_ptr(), 0.01f, -100.0f, 100.0f)) {}
            if (ImGui::DragFloat3("Rotation", active_m->rotation_ptr(), 0.01f, (float)(-M_PI), (float)(M_PI))) {}
            
            // resets the model's transformation
            if (ImGui::Button("Reset")) {
                active_m->reset_transform();
            }
            ImGui::End();

        }
        // renderer viewport
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Renderer", NULL, ImGuiWindowFlags_AlwaysAutoResize);
        static bool resizing = true;
        auto size = ImGui::GetWindowSize();
        dxfb->resize({ size.x, size.y-25 });
        camera.set_aspect_ratio(size.x / size.y);
        ImGuizmo::SetRect(0, 0, size.x, size.y);
        // function that calls an ImGui image with the framebuffer's color stencil data
        dxfb->ImGui_Image();
        ImGui::End();
        ImGui::PopStyleVar();

        ImGui::End();
        Render::ImGui_Render();
        Render::SwapBuffers(use_vsync);
        dt_timer.stop();
        dt = dt_timer.elapsed_ms();

        if (reset) {
            reset = false;
            // TODO: figure this out
            Render::Reset(directxwindow);
            switch (Renderer::get_activeAPI()) {
                case RenderAPI::OPENGL: {
                    dx_shader.reset(Shader::construct(gl_model_shaders.data(), gl_model_shaders.size()));
                    sky_shader.reset(Shader::construct(gl_skybox_shaders.data(), gl_skybox_shaders.size()));
                } break;
                case RenderAPI::DIRECTX11: {
                    dx_shader.reset(Shader::construct(dx_model_shaders.data(), dx_model_shaders.size()));
                    sky_shader.reset(Shader::construct(dx_skybox_shaders.data(), dx_skybox_shaders.size()));
                    }
            }
            skycube.reset(new Mesh(Shape::Cube));
            skycube->get_vertex_buffer()->set_layout({
                {"POSITION", ShaderType::FLOAT3},
                {"UV", ShaderType::FLOAT2},
                {"NORMAL", ShaderType::FLOAT3}
            });
            sky_image.reset(Texture::construct(skyboxes["lake"]));
            dxfb.reset(FrameBuffer::construct({ displays[display].w * 0.80, displays[display].h * 0.80 }));
            dxrb.reset(ResourceBuffer::construct(sizeof(cb_vs)));
            for (auto &m : models) m.reload();
        }

    } // while true loop

    display = SDL_GetWindowDisplayIndex(directxwindow);
    //clean up SDL
    SDL_DestroyWindow(directxwindow);
    SDL_Quit();
} // main
} // namespace Raekor