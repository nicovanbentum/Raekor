#include "pch.h"
#include "apps.h"
#include "input.h"
#include "platform/OS.h"

namespace Raekor {

RayTraceApp::RayTraceApp() : WindowApplication(RendererFlags::OPENGL), renderer(window, viewport) {
    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // gui stuff
    gui::setFont(settings.font.c_str());
    gui::setTheme(settings.themeColors);

    // this is where the timer starts
    rayTracePass = std::make_unique<RenderPass::RayCompute>(viewport);

    activeScreenTexture = rayTracePass->finalResult;

    std::cout << "Initialization done." << std::endl;

    SDL_ShowWindow(window);
    SDL_MaximizeWindow(window);

    viewport.setFov(20);
    viewport.getCamera().move(glm::vec2(-3, 3));
    viewport.getCamera().zoom(19);
    viewport.getCamera().look(-3.3f, .2f);

}

//////////////////////////////////////////////////////////////////////////////////////////////////

void RayTraceApp::update(float dt) {
    //TODO: bool inFreeCameraMode = InputHandler::handleEvents(this, true, dt);
    bool inFreeCameraMode = false;
    viewport.getCamera().update(true);

    // clear the main window
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, viewport.size.x, viewport.size.y);

    rayTracePass->execute(viewport, !inFreeCameraMode && !sceneChanged);

    //get new frame for ImGui and ImGuizmo
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    renderer.ImGui_NewFrame(window);

    sceneChanged = false;

    if (rayTracePass->shaderChanged()) {
        sceneChanged = true;
    }

    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_C), true)) {
        if (SDL_GetModState() & KMOD_LCTRL) {
            rayTracePass->spheres.push_back(rayTracePass->spheres[activeSphere]);
            activeSphere = static_cast<uint32_t>(rayTracePass->spheres.size() - 1);
            sceneChanged = true;
        }
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

    ImGui::Separator(); ImGui::NewLine();

    if (drawSphereProperties(rayTracePass->spheres[activeSphere])) {
        sceneChanged = true;
    }

    if (ImGui::Button("New sphere.."))
        ImGui::OpenPopup("Sphere properties");

    ImGui::SameLine();

    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete), true)) {
        rayTracePass->spheres.erase(rayTracePass->spheres.begin() + activeSphere);
        activeSphere = static_cast<uint32_t>(std::max(0ull, rayTracePass->spheres.size() - 1));
        sceneChanged = true;
    }

    ImGui::Separator();

    bool open = true;
    if (ImGui::BeginPopupModal("Sphere properties", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
        static RenderPass::Sphere sphere = {
            glm::vec3(0, 0, 0), 
            glm::vec3(1, 1, 1),
            1, 1, 1
        };

        drawSphereProperties(sphere);

        if (ImGui::Button("Create")) {
            rayTracePass->spheres.push_back(sphere);
            ImGui::CloseCurrentPopup();
            sceneChanged = true;
            activeSphere = static_cast<uint32_t>(rayTracePass->spheres.size() - 1);
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::End();
    
    cameraSettingsWindow.drawWindow(viewport.getCamera());

    gizmo.drawWindow();

    // renderer viewport
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Renderer", NULL, ImGuiWindowFlags_AlwaysAutoResize);

    // figure out if we need to resize the viewport
    auto size = ImGui::GetContentRegionAvail();
    auto resized = false;
    if (viewport.size.x != size.x || viewport.size.y != size.y) {
        viewport.resize({ size.x, size.y });
        resized = true;
    }

    auto pos = ImGui::GetWindowPos();

    ImGui::Image((void*)((intptr_t)activeScreenTexture), ImVec2((float)viewport.size.x, (float)viewport.size.y), { 0,1 }, { 1,0 });

    // set the gizmo's viewport
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(pos.x, pos.y, (float)viewport.size.x, (float)viewport.size.y);

    // draw gizmo
    glm::vec3 rotation, scale;
    auto matrix = glm::translate(glm::mat4(1.0f), rayTracePass->spheres[activeSphere].origin);
    matrix = glm::scale(matrix, glm::vec3(rayTracePass->spheres[activeSphere].radius));
    auto deltaMatrix = glm::mat4(1.0f);
    
    if (ImGuizmo::Manipulate(glm::value_ptr(viewport.getCamera().getView()), glm::value_ptr(viewport.getCamera().getProjection()),
        gizmo.getOperation(), ImGuizmo::MODE::LOCAL, glm::value_ptr(matrix), glm::value_ptr(deltaMatrix))) {
        // update the transformation
        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(matrix), glm::value_ptr(rayTracePass->spheres[activeSphere].origin),
        glm::value_ptr(rotation),glm::value_ptr(scale));
        float averageScale = (matrix[0][0] + matrix[1][1] + matrix[2][2]) / 3;
        rayTracePass->spheres[activeSphere].radius = averageScale;
        sceneChanged = true;
    }

    auto& io = ImGui::GetIO();
    if (io.MouseClicked[0] && ImGui::IsWindowHovered() && !ImGuizmo::IsOver(ImGuizmo::OPERATION::TRANSLATE)) {
        // get mouse position in window
        glm::ivec2 mousePosition;
        SDL_GetMouseState(&mousePosition.x, &mousePosition.y);

        // get mouse position relative to viewport
        glm::ivec2 rendererMousePosition = { (mousePosition.x - pos.x), (mousePosition.y - pos.y) };

        auto ray = Math::Ray(viewport, rendererMousePosition);

        // check for ray intersection with every sphere 
        float closest_t = FLT_MAX;
        uint32_t closestSphereIndex = activeSphere;
        for (uint32_t sphereIndex = 0; sphereIndex < rayTracePass->spheres.size(); sphereIndex++) {
            const auto& sphere = rayTracePass->spheres[sphereIndex];
            auto hit = ray.hitsSphere(sphere.origin, sphere.radius, 0.001f, 10000.0f);

            if (hit.has_value()) {
            }
            // if it hit a sphere AND the intersection distance is the smallest so far we save it
            if (hit.has_value() && hit.value() < closest_t) {
                closest_t = hit.value();
                closestSphereIndex = sphereIndex;
            }
        }

        if (closestSphereIndex == activeSphere) {
            activeSphere = 0;
        } else {
            activeSphere = closestSphereIndex;
        }
    }

    ImGui::SetNextWindowPos(pos);

    ImGui::End();
    ImGui::PopStyleVar();

    // application/render metrics
    ImGui::SetNextWindowBgAlpha(0.35f);
    auto metricWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize;
    ImGui::Begin("GPU Metrics", (bool*)0, metricWindowFlags);
    ImGui::Text("Vendor: %s", glGetString(GL_VENDOR));
    ImGui::Text("Product: %s", glGetString(GL_RENDERER));
    ImGui::Text("Resolution: %i x %i", viewport.size.x, viewport.size.y);
    ImGui::Text("Frame %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::Text("Graphics API: OpenGL %s", glGetString(GL_VERSION));
    ImGui::End();

    dockspace.end();

    renderer.ImGui_Render();
    
    SDL_GL_SwapWindow(window);

    if (resized) {
        rayTracePass->deleteResources();
        rayTracePass->createResources(viewport);
    }
}

bool RayTraceApp::drawSphereProperties(RenderPass::Sphere& sphere) {
    bool changed = false;
    changed |= ImGui::DragFloat("Radius", &sphere.radius);
    changed |= ImGui::DragFloat3("Position", glm::value_ptr(sphere.origin));
    changed |= ImGui::DragFloat("Roughness", &sphere.roughness, 0.001f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat("Metalness", &sphere.metalness, 1.0f, 0.0f, 1.0f);
    changed |= ImGui::ColorEdit3("Base colour", glm::value_ptr(sphere.colour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    return changed;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

VulkanApp::VulkanApp() : WindowApplication(RendererFlags::VULKAN), vk(window) {
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

void VulkanApp::update(float dt) {
    //handle sdl and imgui events
    // TODO: InputHandler::handleEvents(this, false, dt);

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
