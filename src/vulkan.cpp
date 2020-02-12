#include "pch.h"
#include "app.h"
#include "mesh.h"
#include "util.h"
#include "timer.h"
#include "entry.h"
#include "camera.h"
#include "buffer.h"
#include "PlatformContext.h"
#include "VK/VKRenderer.h"

struct mod {
    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 position = { 0.0, 0.0f, 0.0f }, scale = { 1.0f, 1.0f, 1.0f }, rotation = { 0, 0, 0 };

    void transform() {
        model = glm::mat4(1.0f);
        model = glm::translate(model, position);
        auto rotation_quat = static_cast<glm::quat>(rotation);
        model = model * glm::toMat4(rotation_quat);
        model = glm::scale(model, scale);
    }
};

namespace Raekor {

bool Application::running = true;
bool Application::showUI = true;
bool Application::shouldResize = false;

void Application::vulkan_main() {
    auto context = Raekor::PlatformContext();

    // retrieve the application settings from the config file
    serialize_settings("config.json");

    int sdl_err = SDL_Init(SDL_INIT_VIDEO);
    m_assert(sdl_err == 0, "failed to init SDL for video");

    Uint32 wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN |
        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN;

    std::vector<SDL_Rect> displays;
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        displays.push_back(SDL_Rect());
        SDL_GetDisplayBounds(i, &displays.back());
    }

    // if our display setting is higher than the nr of displays we pick the default display
    auto index = display > displays.size() - 1 ? 0 : display;
    auto window = SDL_CreateWindow(name.c_str(),
        SDL_WINDOWPOS_CENTERED_DISPLAY(index),
        SDL_WINDOWPOS_CENTERED_DISPLAY(index),
        static_cast<int>(displays[index].w * 0.9),
        static_cast<int>(displays[index].h * 0.9),
        wflags);

    //initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigDockingWithShift = true;
    ImFont* pFont = io.Fonts->AddFontFromFileTTF(font.c_str(), 18.0f);
    if (!io.Fonts->Fonts.empty()) {
        io.FontDefault = io.Fonts->Fonts.back();
    }

    Camera camera(glm::vec3(0, 1.0, 0), glm::perspectiveRH_ZO(glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 10000.0f));


    auto face_files = skyboxes["lake"];
    VK::Renderer vk = VK::Renderer(window, face_files);

    vk.ImGuiInit(window);
    vk.ImGuiCreateFonts();

    std::puts("job well done.");

    SDL_ShowWindow(window);
    SDL_SetWindowInputFocus(window);

    Ffilter ft_mesh = {};
    ft_mesh.name = "Supported Mesh Files";
    ft_mesh.extensions = "*.obj;*.fbx";

    // MVP uniform buffer object
    glm::mat4 ubo = {};
    int active = 0;

    std::vector<mod> mods = std::vector<mod>(vk.getMeshCount());
    for (mod& m : mods) {
        m.model = glm::mat4(1.0f);
        m.transform();
    }

    Timer dt_timer = Timer();
    double dt = 0;

    glm::mat4 lightmatrix = glm::mat4(1.0f);
    lightmatrix = glm::translate(lightmatrix, { 0.0, 1.0f, 0.0 });
    float lightPos[3], lightRot[3], lightScale[3];
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(lightmatrix), lightPos, lightRot, lightScale);
    glm::vec4 lightAngle = { 0.0f, 1.0f, 1.0f, 0.0f };

    bool use_vsync = true;
    bool update = false;

    //main application loop
    while (running) {
        dt_timer.start();
        //handle sdl and imgui events
        handle_sdl_gui_events({ window }, camera, dt);

        // update the mvp structs
        for (uint32_t i = 0; i < mods.size(); i++) {
            MVP* modelMat = (MVP*)(((uint64_t)vk.uboDynamic.mvp + (i * vk.dynamicAlignment)));
            modelMat->model = mods[i].model;
            modelMat->projection = camera.getProjection();
            modelMat->view = camera.getView();
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(lightmatrix), lightPos, lightRot, lightScale);
            modelMat->lightPos = glm::vec4(glm::make_vec3(lightPos), 1.0f);
            modelMat->lightAngle = lightAngle;
        }

        uint32_t frame = vk.getNextFrame();

        // start a new imgui frame
        vk.ImGuiNewFrame(window);
        ImGuizmo::BeginFrame();
        ImGuizmo::Enable(true);

        static bool opt_fullscreen_persistant = true;
        bool opt_fullscreen = opt_fullscreen_persistant;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_DockNodeHost;
        if (opt_fullscreen) {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
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
        ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(io.DisplaySize.x, io.DisplaySize.y), dockspace_flags);
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

        if (showUI) {
            // draw the imguizmo at the center of the light
            ImGuizmo::Manipulate(glm::value_ptr(camera.getView()), glm::value_ptr(camera.getProjection()), ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::MODE::WORLD, glm::value_ptr(lightmatrix));
            ImGui::Begin("ECS", (bool*)0, ImGuiWindowFlags_AlwaysAutoResize);
            if (ImGui::Button("Add Model")) {
                std::string path = context.open_file_dialog({ ft_mesh });
                if (!path.empty()) {
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove Model")) {
            }
            ImGui::End();

            ImGui::ShowMetricsWindow();

            ImGui::Begin("Mesh Properties");

            if (ImGui::SliderInt("Mesh", &active, 0, 24)) {}
            if (ImGui::DragFloat3("Scale", glm::value_ptr(mods[active].scale), 0.01f, 0.0f, 10.0f)) {
                mods[active].transform();
            }
            if (ImGui::DragFloat3("Position", glm::value_ptr(mods[active].position), 0.01f, -100.0f, 100.0f)) {
                mods[active].transform();
            }
            if (ImGui::DragFloat3("Rotation", glm::value_ptr(mods[active].rotation), 0.01f, (float)(-M_PI), (float)(M_PI))) {
                mods[active].transform();
            }
            ImGui::End();

            ImGui::Begin("Camera Properties");
            if (ImGui::DragFloat("Camera Move Speed", camera.get_move_speed(), 0.001f, 0.01f, FLT_MAX, "%.2f")) {}
            if (ImGui::DragFloat("Camera Look Speed", camera.get_look_speed(), 0.0001f, 0.0001f, FLT_MAX, "%.4f")) {}

            ImGui::End();

            // scene panel
            ImGui::Begin("Scene");
            // toggle button for vsync
            if (ImGui::RadioButton("USE VSYNC", use_vsync)) {
                use_vsync = !use_vsync;
                update = true;
            }
            if (ImGui::RadioButton("Animate Light", move_light)) {
                move_light = !move_light;
            }

            if (ImGui::Button("Reload shaders")) {
                vk.reloadShaders();
            }

            ImGui::NewLine(); ImGui::Separator();

            ImGui::Text("Light Properties");
            if (ImGui::DragFloat3("Angle", glm::value_ptr(lightAngle), 0.01f, -1.0f, 1.0f)) {}


            ImGui::End();
        }


        // End DOCKSPACE
        ImGui::End();

        // tell imgui to collect render data
        ImGui::Render();
        // record the collected data to secondary command buffers
        vk.ImGuiRecord();
        // start the overall render pass
        camera.update();

        glm::mat4 sky_matrix = camera.getProjection() * glm::mat4(glm::mat3(camera.getView())) * glm::mat4(1.0f);

        vk.render(frame, sky_matrix);
        // tell imgui we're done with the current frame
        ImGui::EndFrame();

        if (update || shouldResize) {
            vk.recreateSwapchain(use_vsync);
            update = false;
            shouldResize = false;
        }

        dt_timer.stop();
        dt = dt_timer.elapsed_ms();
    }

    vk.waitForIdle();

    SDL_DestroyWindow(window);
    SDL_Quit();
}

} // namespace Raekor