#pragma once

#include "application.h"
#include "gui.h"
#include "physics.h"

namespace Raekor {

class EditorOpenGL : public WindowApplication {
public:
    EditorOpenGL();
    virtual ~EditorOpenGL() = default;

    virtual void update(float dt);

private:
    Scene scene;
    entt::entity active = entt::null;
    entt::entity defaultMaterialEntity;

    ecs::MeshComponent quad;

    GLRenderer renderer;

    bool mouseInViewport = false;
    bool doSSAO = false, doBloom = false, debugVoxels = false;
    bool shouldVoxelize = true, gizmoEnabled = false, showSettingsWindow = false;

    Physics physics;
    gui::Guizmo gizmo;
    gui::Dockspace dockspace;
    gui::TopMenuBar topMenuBar;
    gui::EntityWindow ecsWindow;
    gui::AssetWindow assetBrowser;
    gui::RandomWindow randomWindow;
    gui::CameraSettings cameraWindow;
    gui::ViewportWindow viewportWindow;
    gui::InspectorWindow inspectorWindow;
    gui::PostprocessWindow postprocessWindow;
};

} // raekor