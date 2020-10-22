#include "pch.h"
#include "editor.h"
#include "gui.h"
#include "input.h"
#include "platform/OS.h"

namespace Raekor {

EditorOpenGL::EditorOpenGL() : WindowApplication(RenderAPI::OPENGL), renderer(window) {
    // gui stuff
    gui::setFont(settings.font.c_str());
    gui::setTheme(settings.themeColors);

    // create quad for render passes
    for (const auto& v : quadVertices) {
        quad.positions.push_back(v.pos);
        quad.uvs.push_back(v.uv);
        quad.normals.push_back(v.normal);
        quad.tangents.push_back(v.tangent);
        quad.bitangents.push_back(v.binormal);
    }

    for (const auto& i : quadIndices) {
        quad.indices.push_back(i.p1);
        quad.indices.push_back(i.p2);
        quad.indices.push_back(i.p3);
    }

    quad.uploadVertices();
    quad.uploadIndices();

    skinningPass            = std::make_unique<RenderPass::Skinning>();
    voxelizationPass        = std::make_unique<RenderPass::Voxelization>(128);
    shadowMapPass           = std::make_unique<RenderPass::ShadowMap>(4096, 4096);
    tonemappingPass         = std::make_unique<RenderPass::Tonemapping>(viewport);
    geometryBufferPass      = std::make_unique<RenderPass::GeometryBuffer>(viewport);
    fowardLightingPass      = std::make_unique<RenderPass::ForwardLighting>(viewport);
    DeferredLightingPass    = std::make_unique<RenderPass::DeferredLighting>(viewport);
    boundingBoxDebugPass    = std::make_unique<RenderPass::BoundingBoxDebug>(viewport);
    voxelizationDebugPass   = std::make_unique<RenderPass::VoxelizationDebug>(viewport);

    // keep a pointer to the texture that's rendered to the window
    activeScreenTexture = tonemappingPass->result;

    std::cout << "Initialization done." << std::endl;

    SDL_ShowWindow(window);
    SDL_MaximizeWindow(window);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void EditorOpenGL::update(double dt) {
    InputHandler::handleEvents(this, mouseInViewport, dt);

    scene.updateTransforms();
    
    auto animationView = scene->view<ecs::MeshAnimationComponent>();
    std::for_each(std::execution::par_unseq, animationView.begin(), animationView.end(), [&](auto entity) {
        auto& animation = animationView.get<ecs::MeshAnimationComponent>(entity);
        animation.boneTransform(static_cast<float>(dt));
    });

    scene->view<ecs::MeshAnimationComponent, ecs::MeshComponent>().each([&](auto& animation, auto& mesh) {
        skinningPass->execute(mesh, animation);
    });

    viewport.getCamera().update(true);

    // clear the main window
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // generate sun shadow map 
    glViewport(0, 0, 4096, 4096);
    shadowMapPass->execute(scene);

    if (shouldVoxelize) {
        voxelizationPass->execute(scene, viewport, shadowMapPass.get());
    }

    glViewport(0, 0, viewport.size.x, viewport.size.y);

    if (doDeferred) {
        if (!scene->view<ecs::MeshComponent>().empty()) {
            geometryBufferPass->execute(scene, viewport);
            DeferredLightingPass->execute(scene, viewport, shadowMapPass.get(), nullptr, geometryBufferPass.get(), nullptr, voxelizationPass.get(), quad);
            tonemappingPass->execute(DeferredLightingPass->result, quad);
        }
    }
    else {
        if (!scene->view<ecs::MeshComponent>().empty()) {
            fowardLightingPass->execute(viewport, scene, voxelizationPass.get(), shadowMapPass.get());
            tonemappingPass->execute(fowardLightingPass->result, quad);
        }
    }

    if (active != entt::null) {
        boundingBoxDebugPass->execute(scene, viewport, tonemappingPass->result, geometryBufferPass->GDepthBuffer, active);
    }

    if (debugVoxels) {
        voxelizationDebugPass->execute(viewport, tonemappingPass->result, voxelizationPass.get());
    }

    //get new frame for ImGui and ImGuizmo
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    renderer.ImGui_NewFrame(window);
    ImGuizmo::BeginFrame();

    if (ImGui::IsAnyItemActive()) {
        // perform input mapping
    }

    dockspace.begin();

    topMenuBar.draw(this, scene, activeScreenTexture, active);
    
    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete), true)) {
        if (active != entt::null) {
            if (scene->has<ecs::NodeComponent>(active)) {
                scene.destroyObject(active);
            }
            else {
                scene->destroy(active);
            }
            active = entt::null;
        }
    }

    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_C), true)) {
        if (SDL_GetModState() & KMOD_LCTRL) {
            auto newEntity = scene->create();

            scene->visit(active, [&](const auto& component) {
                auto clone_function = ecs::cloner::getSingleton()->getFunction(component);
                if (clone_function) {
                    clone_function(scene, active, newEntity);
                }
            });
        }
    }

    assetBrowser.drawWindow(scene, active);

    inspectorWindow.draw(scene, active);

    ecsWindow.draw(scene, active);

    // post processing panel
    ImGui::Begin("Post Processing");
    
    ImGui::Separator();

    if (ImGui::SliderFloat("Exposure", &tonemappingPass->settings.exposure, 0.0f, 1.0f)) {}
    if (ImGui::SliderFloat("Gamma", &tonemappingPass->settings.gamma, 1.0f, 3.2f)) {}
    ImGui::NewLine();

    if (ImGui::Checkbox("Bloom", &doBloom)) {}
    ImGui::Separator();

    if (ImGui::DragFloat3("Threshold", glm::value_ptr(DeferredLightingPass->settings.bloomThreshold), 0.001f, 0.0f, 10.0f)) {}
    ImGui::NewLine();

    ImGui::End();

    // scene panel
    ImGui::Begin("Random");
    ImGui::SetItemDefaultFocus();

    // toggle button for openGl vsync
    static bool doVsync = true;
    if (ImGui::RadioButton("Vsync", doVsync)) {
        doVsync = !doVsync;
    }

    ImGui::NewLine(); ImGui::Separator();
    ImGui::Text("Voxel Cone Tracing");

    if (ImGui::RadioButton("Debug", debugVoxels)) {
        debugVoxels = !debugVoxels;
    }

    if (ImGui::RadioButton("Voxelize", shouldVoxelize)) {
        shouldVoxelize = !shouldVoxelize;
    }

    if (ImGui::RadioButton("Deferred", doDeferred)) {
        doDeferred = !doDeferred;
    }

    ImGui::DragFloat("World size", &voxelizationPass->worldSize, 0.05f, 1.0f, FLT_MAX, "%.2f");

    ImGui::Separator();

    if (ImGui::TreeNode("Screen Texture")) {
        if (ImGui::Selectable(nameof(tonemappingPass->result), activeScreenTexture == tonemappingPass->result))
            activeScreenTexture = tonemappingPass->result;
        if (ImGui::Selectable(nameof(geometryBufferPass->albedoTexture), activeScreenTexture == geometryBufferPass->albedoTexture))
            activeScreenTexture = geometryBufferPass->albedoTexture;
        if (ImGui::Selectable(nameof(geometryBufferPass->normalTexture), activeScreenTexture == geometryBufferPass->normalTexture))
            activeScreenTexture = geometryBufferPass->normalTexture;
        if (ImGui::Selectable(nameof(geometryBufferPass->materialTexture), activeScreenTexture == geometryBufferPass->materialTexture))
            activeScreenTexture = geometryBufferPass->materialTexture;
        if (ImGui::Selectable(nameof(geometryBufferPass->positionTexture), activeScreenTexture == geometryBufferPass->positionTexture))
            activeScreenTexture = geometryBufferPass->positionTexture;
        ImGui::TreePop();
    }

    ImGui::NewLine();

    ImGui::Text("Shadow Mapping");
    ImGui::Separator();

    if (ImGui::DragFloat2("Planes", glm::value_ptr(shadowMapPass->settings.planes), 0.1f)) {}
    if (ImGui::DragFloat("Size", &shadowMapPass->settings.size)) {}
    if (ImGui::DragFloat("Min bias", &DeferredLightingPass->settings.minBias, 0.0001f, 0.0f, FLT_MAX, "%.4f")) {}
    if (ImGui::DragFloat("Max bias", &DeferredLightingPass->settings.maxBias, 0.0001f, 0.0f, FLT_MAX, "%.4f")) {}

    ImGui::NewLine();
    ImGui::Separator();
    ImGui::NewLine();

    ImGui::End();

    ImGui::Begin("Camera Properties");
    if (ImGui::DragFloat("FoV", &viewport.getFov(), 1.0f, 35.0f, 120.0f)) {
        viewport.setFov(viewport.getFov());
    }
    if (ImGui::DragFloat("Move Speed", &viewport.getCamera().moveSpeed, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}
    if (ImGui::DragFloat("Move Constant", &viewport.getCamera().moveConstant, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}
    if (ImGui::DragFloat("Look Speed", &viewport.getCamera().lookSpeed, 0.1f, 0.0001f, FLT_MAX, "%.4f")) {}
    if (ImGui::DragFloat("Look Constant", &viewport.getCamera().lookConstant, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}
    if (ImGui::DragFloat("Zoom Speed", &viewport.getCamera().zoomSpeed, 0.001f, 0.0001f, FLT_MAX, "%.4f")) {}
    if (ImGui::DragFloat("Zoom Constant", &viewport.getCamera().zoomConstant, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}

    ImGui::End();

    gizmo.drawWindow();

    // renderer viewport

    bool resized = viewportWindow.begin(viewport, activeScreenTexture);

    auto pos = ImGui::GetWindowPos();
    auto size = ImGui::GetWindowSize();
    mouseInViewport = ImGui::IsWindowHovered();

    auto& io = ImGui::GetIO();
    if (io.MouseClicked[0] && mouseInViewport && !(active != entt::null && ImGuizmo::IsOver(gizmo.getOperation()))) {
        // get mouse position in window
        glm::ivec2 mousePosition;
        SDL_GetMouseState(&mousePosition.x, &mousePosition.y);

        // get mouse position relative to viewport
        auto pos = ImGui::GetWindowPos();
        glm::ivec2 rendererMousePosition = { (mousePosition.x - pos.x), (mousePosition.y - pos.y) };

        // flip mouse coords for opengl
        rendererMousePosition.y = viewport.size.y - rendererMousePosition.y;

        entt::entity picked = entt::null;
        if (doDeferred) {
            picked = geometryBufferPass->pick(rendererMousePosition.x, rendererMousePosition.y);
        } else {
            picked = fowardLightingPass->pick(rendererMousePosition.x, rendererMousePosition.y);
        }

        if (scene->valid(picked)) {
            active = active == picked ? entt::null : picked;
        } else {
            active = entt::null;
        }
    }

    if (active != entt::null) {
        viewportWindow.drawGizmo(gizmo, scene, viewport, active);
    }

    viewportWindow.end();

    const auto metricsWindowSize = ImVec2(size.x / 7.5f, size.y / 9.0f);
    const auto metricsWindowPosition = ImVec2(pos.x + size.x - size.x / 7.5f - 5.0f, pos.y + 5.0f);
    metricsWindow.draw(viewport, metricsWindowPosition, metricsWindowSize);

    dockspace.end();

    renderer.ImGui_Render();
    renderer.SwapBuffers(window, doVsync);

    if (resized) {
        // resizing framebuffers
        DeferredLightingPass->deleteResources();
        DeferredLightingPass->createResources(viewport);

        fowardLightingPass->deleteResources();
        fowardLightingPass->createResources(viewport);

        boundingBoxDebugPass->deleteResources();
        boundingBoxDebugPass->createResources(viewport);

        voxelizationDebugPass->deleteResources();
        voxelizationDebugPass->createResources(viewport);

        tonemappingPass->deleteResources();
        tonemappingPass->createResources(viewport);

        geometryBufferPass->deleteResources();
        geometryBufferPass->createResources(viewport);
    }
}

} // raekor