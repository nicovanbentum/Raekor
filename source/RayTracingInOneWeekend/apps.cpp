#include "Editor/pch.h"
#include "apps.h"
#include "Raekor/input.h"
#include "Raekor/OS.h"
#include "Editor/viewportWidget.h"
#include "Editor/renderpass.h"

namespace Raekor {

RayTraceApp::RayTraceApp() : Application(WindowFlag::OPENGL), renderer(m_Window, m_Viewport) {
    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // gui stuff
    GUI::SetFont(m_Settings.mFontFile);
    GUI::SetTheme();

    // this is where the timer starts
    rayTracePass = std::make_unique<RayTracingOneWeekend>(m_Viewport);

    activeScreenTexture = rayTracePass->finalResult;

    std::cout << "Initialization done." << std::endl;

    SDL_ShowWindow(m_Window);
    SDL_MaximizeWindow(m_Window);

    // TODO: viewport.setFov(20);
    m_Viewport.GetCamera().Move(glm::vec2(-3, 3));
    m_Viewport.GetCamera().Zoom(19);
    m_Viewport.GetCamera().Look(Vec2(-3.3f, .2f));
}



void RayTraceApp::OnUpdate(float dt) {
    //TODO: bool inFreeCameraMode = InputHandler::handleEvents(this, true, dt);
    bool inFreeCameraMode = false;
    m_Viewport.OnUpdate(dt);

    // clear the main window
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, m_Viewport.size.x, m_Viewport.size.y);

    rayTracePass->compute(m_Viewport, !inFreeCameraMode && !sceneChanged);

    //get new frame for ImGui and ImGuizmo
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();

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

    ImGuiWindowFlags dockWindowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiViewport* imGuiViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(imGuiViewport->Pos);
    ImGui::SetNextWindowSize(imGuiViewport->Size);
    ImGui::SetNextWindowViewport(imGuiViewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    dockWindowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) {
        dockWindowFlags |= ImGuiWindowFlags_NoBackground;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", (bool*)true, dockWindowFlags);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }
    ImGui::Begin("Settings");

    if (ImGui::TreeNode("Screen Texture")) {
        if (ImGui::Selectable("rayTracePass->result", activeScreenTexture == rayTracePass->result))
            activeScreenTexture = rayTracePass->result;
        if (ImGui::Selectable("rayTracePass->finalResult", activeScreenTexture == rayTracePass->finalResult))
            activeScreenTexture = rayTracePass->finalResult;
        ImGui::TreePop();
    }

    if (ImGui::Button("Save screenshot")) {
        auto savePath = OS::sSaveFileDialog("Uncompressed PNG (*.png)\0", "png");

        if (!savePath.empty()) {
            const auto bufferSize = 4 * m_Viewport.size.x * m_Viewport.size.y;
            auto pixels = std::vector<unsigned char>(bufferSize);
            
            glGetTextureImage(activeScreenTexture, 0, GL_RGBA, GL_UNSIGNED_BYTE, bufferSize * sizeof(unsigned char), pixels.data());
            
            stbi_flip_vertically_on_write(true);
            stbi_write_png(savePath.c_str(), m_Viewport.size.x, m_Viewport.size.y, 4, pixels.data(), m_Viewport.size.x * 4);
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
        static Sphere sphere = {
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
    
    // renderer viewport
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Renderer", NULL, ImGuiWindowFlags_AlwaysAutoResize);

    // figure out if we need to resize the viewport
    auto size = ImGui::GetContentRegionAvail();
    auto resized = false;
    if (m_Viewport.size.x != size.x || m_Viewport.size.y != size.y) {
        m_Viewport.SetSize({ size.x, size.y });
        resized = true;
    }

    auto pos = ImGui::GetWindowPos();

    ImGui::Image((void*)((intptr_t)activeScreenTexture), ImVec2((float)m_Viewport.size.x, (float)m_Viewport.size.y), { 0,1 }, { 1,0 });

    // set the gizmo's viewport
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(pos.x, pos.y, (float)m_Viewport.size.x, (float)m_Viewport.size.y);

    // draw gizmo
    glm::vec3 rotation, scale;
    auto matrix = glm::translate(glm::mat4(1.0f), rayTracePass->spheres[activeSphere].origin);
    matrix = glm::scale(matrix, glm::vec3(rayTracePass->spheres[activeSphere].radius));
    auto deltaMatrix = glm::mat4(1.0f);
    
    if (ImGuizmo::Manipulate(glm::value_ptr(m_Viewport.GetCamera().GetView()), glm::value_ptr(m_Viewport.GetCamera().GetProjection()),
        ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::MODE::LOCAL, glm::value_ptr(matrix), glm::value_ptr(deltaMatrix))) {
        // update the transformation
        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(matrix), glm::value_ptr(rayTracePass->spheres[activeSphere].origin),
        glm::value_ptr(rotation),glm::value_ptr(scale));
        float averageScale = (matrix[0][0] + matrix[1][1] + matrix[2][2]) / 3;
        rayTracePass->spheres[activeSphere].radius = averageScale;
        sceneChanged = true;
    }

    if (io.MouseClicked[0] && ImGui::IsWindowHovered() && !ImGuizmo::IsOver(ImGuizmo::OPERATION::TRANSLATE)) {
        // get mouse position in window
        glm::ivec2 mousePosition;
        SDL_GetMouseState(&mousePosition.x, &mousePosition.y);

        // get mouse position relative to viewport
        glm::ivec2 rendererMousePosition = { (mousePosition.x - pos.x), (mousePosition.y - pos.y) };

        auto ray = Ray(m_Viewport, rendererMousePosition);

        // check for ray intersection with every sphere 
        float closest_t = FLT_MAX;
        uint32_t closestSphereIndex = activeSphere;
        for (uint32_t sphereIndex = 0; sphereIndex < rayTracePass->spheres.size(); sphereIndex++) {
            const auto& sphere = rayTracePass->spheres[sphereIndex];
            auto hit = ray.HitsSphere(sphere.origin, sphere.radius, 0.001f, 10000.0f);

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
    ImGui::Text("Resolution: %i x %i", m_Viewport.size.x, m_Viewport.size.y);
    ImGui::Text("Frame %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::Text("Graphics API: OpenGL %s", glGetString(GL_VERSION));
    ImGui::End();

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    SDL_GL_SwapWindow(m_Window);

    if (resized) {
        rayTracePass->DestroyRenderTargets();
        rayTracePass->CreateRenderTargets(m_Viewport);
    }
}



bool RayTraceApp::drawSphereProperties(Sphere& sphere) {
    bool changed = false;
    changed |= ImGui::DragFloat("Radius", &sphere.radius);
    changed |= ImGui::DragFloat3("Position", glm::value_ptr(sphere.origin));
    changed |= ImGui::DragFloat("Roughness", &sphere.roughness, 0.001f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat("Metalness", &sphere.metalness, 1.0f, 0.0f, 1.0f);
    changed |= ImGui::ColorEdit3("Base colour", glm::value_ptr(sphere.colour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    return changed;
}



inline double random_double() {
    static std::uniform_real_distribution<double> distribution(0.0, 1.0);
    static std::mt19937 generator;
    return distribution(generator);
}



inline double random_double(double min, double max) {
    static std::uniform_real_distribution<double> distribution(min, max);
    static std::mt19937 generator;
    return distribution(generator);
}



inline glm::vec3 random_color() {
    return glm::vec3(random_double(), random_double(), random_double());
}



inline glm::vec3 random_color(double min, double max) {
    return glm::vec3(random_double(min, max), random_double(min, max), random_double(min, max));
}



RayTracingOneWeekend::RayTracingOneWeekend(const Viewport& viewport) {
    shader.Compile({ {Shader::Type::COMPUTE, "assets\\system\\shaders\\OpenGL\\ray.comp"} });

    spheres.push_back(Sphere{ glm::vec3(0, -1000, 0), glm::vec3(0.5, 0.5, 0.5), 1.0f, 0.0f, 1000.0f });

    int count = 3;
    for (int a = -count; a < count; a++) {
        for (int b = -count; b < count; b++) {
            auto choose_mat = random_double();
            glm::vec3 center(a + 0.9 * random_double(), 0.2, b + 0.9 * random_double());

            if ((center - glm::vec3(4, 0.2, 0)).length() > 0.9) {
                Sphere sphere;

                if (choose_mat < 0.8) {
                    // diffuse
                    sphere.colour = random_color() * random_color();
                    sphere.metalness = 0.0f;
                    sphere.radius = 0.2f;
                    sphere.origin = center;
                    spheres.push_back(sphere);
                }
                else if (choose_mat < 0.95) {
                    // metal
                    sphere.colour = random_color(0.5, 1);
                    sphere.roughness = static_cast<float>(random_double(0, 0.5));
                    sphere.metalness = 1.0f;
                    sphere.radius = 0.2f;
                    sphere.origin = center;
                    spheres.push_back(sphere);
                }
            }
        }
    }

    spheres.push_back(Sphere{ glm::vec3(-4, 1, 0), glm::vec3(0.4, 0.2, 0.1), 1.0f, 0.0f, 1.0f });
    spheres.push_back(Sphere{ glm::vec3(4, 1, 0), glm::vec3(0.7, 0.6, 0.5), 0.0f, 1.0f, 1.0f });

    CreateRenderTargets(viewport);
    auto clearColour = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    glClearTexImage(result, 0, GL_RGBA, GL_FLOAT, glm::value_ptr(clearColour));

    glCreateBuffers(1, &uniformBuffer);
    glNamedBufferStorage(uniformBuffer, sizeof(uniforms), NULL, GL_DYNAMIC_STORAGE_BIT);
}



RayTracingOneWeekend::~RayTracingOneWeekend() {
    DestroyRenderTargets();
    glDeleteBuffers(1, &sphereBuffer);
    glDeleteBuffers(1, &uniformBuffer);

}



void RayTracingOneWeekend::compute(const Viewport& viewport, bool update) {
    // if the shader changed or we moved the m_Camera we clear the result
    if (!update) {
        glDeleteBuffers(1, &sphereBuffer);
        glCreateBuffers(1, &sphereBuffer);
        glNamedBufferStorage(sphereBuffer, spheres.size() * sizeof(Sphere), spheres.data(), GL_DYNAMIC_STORAGE_BIT);
    }

    shader.Bind();
    glBindImageTexture(0, result, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
    glBindImageTexture(1, finalResult, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, sphereBuffer);

    uniforms.iTime = rayTimer.GetElapsedTime();
    uniforms.position = glm::vec4(viewport.GetCamera().GetPosition(), 1.0);
    uniforms.projection = viewport.GetCamera().GetProjection();
    uniforms.view = viewport.GetCamera().GetView();
    uniforms.doUpdate = update;

    const GLuint numberOfSpheres = static_cast<GLuint>(spheres.size());
    uniforms.sphereCount = numberOfSpheres;

    glNamedBufferSubData(uniformBuffer, 0, sizeof(uniforms), &uniforms);
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, uniformBuffer);

    glDispatchCompute(viewport.size.x / 16, viewport.size.y / 16, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}



void RayTracingOneWeekend::CreateRenderTargets(const Viewport& viewport) {
    glCreateTextures(GL_TEXTURE_2D, 1, &result);
    glTextureStorage2D(result, 1, GL_RGBA32F, viewport.size.x, viewport.size.y);
    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateTextures(GL_TEXTURE_2D, 1, &finalResult);
    glTextureStorage2D(finalResult, 1, GL_RGBA32F, viewport.size.x, viewport.size.y);
    glTextureParameteri(finalResult, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(finalResult, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateBuffers(1, &sphereBuffer);
    glNamedBufferStorage(sphereBuffer, spheres.size() * sizeof(Sphere), spheres.data(), GL_DYNAMIC_STORAGE_BIT);
}



void RayTracingOneWeekend::DestroyRenderTargets() {
    glDeleteTextures(1, &result);
    glDeleteTextures(1, &finalResult);
}

} // raekor
