#include "pch.h"
#include "randomWidget.h"
#include "editor.h"
#include "renderer.h"

namespace Raekor {

RandomWidget::RandomWidget(Editor* editor) :
    IWidget(editor, "Random")
{}

void RandomWidget::draw(float dt) {
    auto& renderer = IWidget::renderer();

    ImGui::Begin(title.c_str());
    ImGui::SetItemDefaultFocus();

    if (ImGui::Checkbox("Vsync", (bool*)(&renderer.settings.vsync))) {
        SDL_GL_SetSwapInterval(renderer.settings.vsync);
    }

    ImGui::SameLine();

    if (ImGui::Checkbox("TAA", (bool*)(&renderer.settings.enableTAA))) {
        renderer.frameNr = 0;
    }

    ImGui::NewLine(); ImGui::Separator();
    ImGui::Text("Voxel Cone Tracing");

    ImGui::DragFloat("Range", &renderer.voxelize->worldSize, 0.05f, 1.0f, FLT_MAX, "%.2f");

    ImGui::NewLine(); ImGui::Separator();

    ImGui::Text("Shadow Mapping");
    ImGui::Separator();

    if (ImGui::DragFloat("Bias constant", &renderer.shadowMaps->settings.depthBiasConstant, 0.01f, 0.0f, FLT_MAX, "%.2f")) {}
    if (ImGui::DragFloat("Bias slope factor", &renderer.shadowMaps->settings.depthBiasSlope, 0.01f, 0.0f, FLT_MAX, "%.2f")) {}
    if (ImGui::DragFloat("Cascade lambda", &renderer.shadowMaps->settings.cascadeSplitLambda, 0.0001f, 0.0f, 1.0f, "%.4f")) {
        renderer.shadowMaps->updatePerspectiveConstants(editor->getViewport());
    }

    ImGui::DragFloat3("Bloom threshold", glm::value_ptr(renderer.deferShading->settings.bloomThreshold), 0.01f, 0.0f, 10.0f, "%.3f");

    float fov = editor->getViewport().getCamera().getFOV();
    if (ImGui::DragFloat("Camera FOV", &fov, 1.0f, 5.0f, 105.0f, "%.2f", 1.0f)) {
    }

    ImGui::End();
}

} // raekor

