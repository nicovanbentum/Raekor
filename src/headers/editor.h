#pragma once

#include "application.h"
#include "gui.h"
#include "physics.h"

namespace Raekor {

class EditorOpenGL : public WindowApplication {
public:
    EditorOpenGL();
    virtual ~EditorOpenGL() = default;

    virtual void update(double dt);

private:
    Scene scene;
    entt::entity active = entt::null;
    unsigned int activeScreenTexture;
    entt::entity defaultMaterialEntity;

    ecs::MeshComponent quad;

    GLRenderer renderer;
    std::unique_ptr<RenderPass::Bloom>              bloomPass;
    std::unique_ptr<RenderPass::Skinning>           skinningPass;
    std::unique_ptr<RenderPass::ShadowMap>          shadowMapPass;
    std::unique_ptr<RenderPass::Tonemapping>        tonemappingPass;
    std::unique_ptr<RenderPass::Voxelization>       voxelizationPass;
    std::unique_ptr<RenderPass::GeometryBuffer>     geometryBufferPass;
    std::unique_ptr<RenderPass::DeferredLighting>   DeferredLightingPass;
    std::unique_ptr<RenderPass::BoundingBoxDebug>   boundingBoxDebugPass;
    std::unique_ptr<RenderPass::VoxelizationDebug>  voxelizationDebugPass;

    bool mouseInViewport = false;
    bool doSSAO = false, doBloom = false, debugVoxels = false;
    bool shouldVoxelize = true, gizmoEnabled = false, showSettingsWindow = false;

    Physics physics;
    gui::Guizmo gizmo;
    gui::Dockspace dockspace;
    gui::TopMenuBar topMenuBar;
    gui::EntityWindow ecsWindow;
    gui::AssetWindow assetBrowser;
    gui::MetricsWindow metricsWindow;
    gui::ViewportWindow viewportWindow;
    gui::InspectorWindow inspectorWindow;
};

} // raekor