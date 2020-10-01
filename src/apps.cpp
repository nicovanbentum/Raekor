#include "pch.h"
#include "apps.h"
#include "input.h"
#include "platform/OS.h"

namespace Raekor {

RayTraceApp::RayTraceApp() : WindowApplication(RenderAPI::OPENGL) {
    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // create the renderer object that does sets up the API and does all the runtime magic
    Renderer::setAPI(RenderAPI::OPENGL);
    Renderer::Init(window);

    // gui stuff
    gui::setFont(settings.font.c_str());
    gui::setTheme(settings.themeColors);

    // this is where the timer starts
    rayTracePass = std::make_unique<RenderPass::RayCompute>(viewport);

    activeScreenTexture = rayTracePass->finalResult;

    viewport.getCamera().zoom(-5);

    std::cout << "Initialization done." << std::endl;

    SDL_ShowWindow(window);
    SDL_MaximizeWindow(window);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void RayTraceApp::update(double dt) {
    bool inFreeCameraMode = InputHandler::handleEvents(this, true, dt);
    viewport.getCamera().update(true);

    // clear the main window
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, viewport.size.x, viewport.size.y);

    rayTracePass->execute(viewport, !inFreeCameraMode && !sceneChanged);

    //get new frame for ImGui and ImGuizmo
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    Renderer::ImGuiNewFrame(window);

    sceneChanged = false;
    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete), true)) {
        rayTracePass->spheres.erase(rayTracePass->spheres.begin() + activeSphere);
        activeSphere = std::max(rayTracePass->spheres.size() -1, 0ull);
        sceneChanged = true;
    }

    dockspace.begin();
    ImGui::Begin("Settings");

    if (ImGui::TreeNode("Screen Texture")) {
        if (ImGui::Selectable(nameof(rayTracePass->result), activeScreenTexture == rayTracePass->result))
            activeScreenTexture = rayTracePass->result;
        if (ImGui::Selectable(nameof(rayTracePass->finalResult), activeScreenTexture == rayTracePass->finalResult))
            activeScreenTexture = rayTracePass->finalResult;
        ImGui::TreePop();
    }

    if (ImGui::Button("Save screenshot")) {
        auto savePath = OS::saveFileDialog("Uncompressed PNG (*.png)\0", "png");

        if (!savePath.empty()) {
            const auto bufferSize = 4 * viewport.size.x * viewport.size.y;
            auto pixels = std::vector<unsigned char>(bufferSize);
            
            glGetTextureImage(activeScreenTexture, 0, GL_RGBA, GL_UNSIGNED_BYTE, bufferSize * sizeof(unsigned char), pixels.data());
            
            stbi_flip_vertically_on_write(true);
            stbi_write_png(savePath.c_str(), viewport.size.x, viewport.size.y, 4, pixels.data(), viewport.size.x * 4);
        }
    }

ImGui::Separator();

    std::vector<std::string> sphereNames;
    for (size_t i = 0; i < rayTracePass->spheres.size(); i++) {
        sphereNames.push_back("Sphere " + std::to_string(i));
    }
    if (ImGui::BeginCombo("Spheres", sphereNames[activeSphere].c_str())) {
        for (uint32_t i = 0; i < sphereNames.size(); i++) {
            bool selected = i == activeSphere;
            if (ImGui::Selectable(sphereNames[i].c_str(), &selected)) {
                activeSphere = i;
            }
        }

        ImGui::EndCombo();
    }

    if (ImGui::Button("New"))
        ImGui::OpenPopup("Sphere properties");

    ImGui::SameLine();

    if (ImGui::Button("Delete") && !rayTracePass->spheres.size() > 1) {
        rayTracePass->spheres.erase(rayTracePass->spheres.begin() + activeSphere);
        activeSphere = std::max(0ull, rayTracePass->spheres.size() - 1);

    }



    ImGui::Separator();

    if (ImGui::BeginPopupModal("Sphere properties", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        static RenderPass::Sphere sphere = {
            glm::vec3(0, 0, 0), 
            glm::vec3(1, 1, 1),
            1, 1, 1
        };

        drawSphereProperties(sphere);

        if (ImGui::Button("Add")) {
            rayTracePass->spheres.push_back(sphere);
            ImGui::CloseCurrentPopup();
            sceneChanged = true;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::End();

    auto resized = viewportWindow.begin(viewport, activeScreenTexture);

    auto pos = ImGui::GetWindowPos();
    auto size = ImGui::GetWindowSize();
    const auto metricsWindowSize = ImVec2(size.x / 7.5f, size.y / 9.0f);
    const auto metricsWindowPosition = ImVec2(pos.x + size.x - size.x / 7.5f - 5.0f, pos.y + 5.0f);
    metricsWindow.draw(viewport, metricsWindowPosition, metricsWindowSize);

    // set the gizmo's viewport
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(pos.x, pos.y, (float)viewport.size.x, (float)viewport.size.y);


    // draw gizmo
    glm::vec3 rotation, scale;
    auto matrix = glm::translate(glm::mat4(1.0f), rayTracePass->spheres[activeSphere].origin);
    
    ImGuizmo::Manipulate(
        glm::value_ptr(viewport.getCamera().getView()),
        glm::value_ptr(viewport.getCamera().getProjection()),
        ImGuizmo::TRANSLATE, ImGuizmo::MODE::LOCAL,
        glm::value_ptr(matrix)
    );

    // update the transformation
    ImGuizmo::DecomposeMatrixToComponents(
        glm::value_ptr(matrix),
        glm::value_ptr(rayTracePass->spheres[activeSphere].origin),
        glm::value_ptr(rotation),
        glm::value_ptr(scale)
    );

    viewportWindow.end();

    cameraSettingsWindow.drawWindow(viewport.getCamera());

    dockspace.end();

    Renderer::ImGuiRender();
    Renderer::SwapBuffers(true);

    if (resized) {
        rayTracePass->deleteResources();
        rayTracePass->createResources(viewport);
    }
}

void RayTraceApp::drawSphereProperties(RenderPass::Sphere& sphere) {
    ImGui::DragFloat3("Position", glm::value_ptr(sphere.origin));
    ImGui::ColorEdit3("Base colour", glm::value_ptr(sphere.colour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    ImGui::DragFloat("Roughness", &sphere.roughness);
    ImGui::DragFloat("Metalness", &sphere.metalness);
    ImGui::DragFloat("Radius", &sphere.radius);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

VulkanApp::VulkanApp() : WindowApplication(RenderAPI::VULKAN), vk(window) {
    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    vk.ImGuiInit(window);
    vk.ImGuiCreateFonts();

    // gui stuff
    gui::setTheme(settings.themeColors);

    // MVP uniform buffer object
    glm::mat4 ubo = {};
    int active = 0;

    mods.resize(vk.getMeshCount());
    for (mod& m : mods) {
        m.model = glm::mat4(1.0f);
        m.transform();
    }

    std::puts("Job well done.");

    SDL_ShowWindow(window);
    SDL_SetWindowInputFocus(window);

}

//////////////////////////////////////////////////////////////////////////////////////////////////

void VulkanApp::update(double dt) {
    //handle sdl and imgui events
    InputHandler::handleEvents(this, false, dt);

    // update the mvp structs
    auto& camera = viewport.getCamera();
    for (uint32_t i = 0; i < mods.size(); i++) {
        MVP* modelMat = (MVP*)(((uint64_t)vk.uboDynamic.mvp + (i * vk.dynamicAlignment)));
        modelMat->model = mods[i].model;
        modelMat->projection = camera.getProjection();
        modelMat->view = camera.getView();
        modelMat->lightPos = glm::vec4(glm::vec3(0, 3, 0), 1.0f);
        modelMat->lightAngle = { 0.0f, 1.0f, 1.0f, 0.0f };
    }

    uint32_t frame = vk.getNextFrame();

    // start a new imgui frame
    vk.ImGuiNewFrame(window);
    ImGuizmo::BeginFrame();
    ImGuizmo::Enable(true);

    dockspace.begin();

    ImGui::Begin("ECS", (bool*)0, ImGuiWindowFlags_AlwaysAutoResize);
    if (ImGui::Button("Add Model")) {
        std::string path = OS::openFileDialog("Supported Files(*.gltf, *.fbx, *.obj)\0*.gltf;*.fbx;*.obj\0");
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
    if (ImGui::DragFloat("Camera Move Speed", &camera.moveSpeed, 0.001f, 0.01f, FLT_MAX, "%.2f")) {}
    if (ImGui::DragFloat("Camera Look Speed", &camera.lookSpeed, 0.0001f, 0.0001f, FLT_MAX, "%.4f")) {}

    ImGui::End();

    // scene panel
    ImGui::Begin("Scene");
    // toggle button for vsync
    if (ImGui::RadioButton("USE VSYNC", useVsync)) {
        useVsync = !useVsync;
        shouldRecreateSwapchain = true;
    }

    if (ImGui::Button("Reload shaders")) {
        vk.reloadShaders();
    }

    ImGui::NewLine(); ImGui::Separator();

    ImGui::End();


    // End DOCKSPACE
    dockspace.end();

    // tell imgui to collect render data
    ImGui::Render();
    // record the collected data to secondary command buffers
    vk.ImGuiRecord();
    // start the overall render pass
    camera.update(false);

    glm::mat4 sky_matrix = camera.getProjection() * glm::mat4(glm::mat3(camera.getView())) * glm::mat4(1.0f);

    vk.render(frame, sky_matrix);
    // tell imgui we're done with the current frame
    ImGui::EndFrame();

    if (shouldRecreateSwapchain || shouldResize) {
        vk.recreateSwapchain(useVsync);
        shouldRecreateSwapchain = false;
        shouldResize = false;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

VulkanApp::~VulkanApp() {
    vk.waitForIdle();
}

} // raekor
