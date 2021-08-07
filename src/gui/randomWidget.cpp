#include "pch.h"
#include "randomWidget.h"
#include "editor.h"
#include "renderer.h"

namespace Raekor {

RandomWidget::RandomWidget(Editor* editor) :
    IWidget(editor, "Random")
{}

void RandomWidget::draw() {
    auto& renderer = IWidget::renderer();

    ImGui::Begin(title.c_str());
    ImGui::SetItemDefaultFocus();

    if (ImGui::RadioButton("Vsync", renderer.vsync)) {
        renderer.vsync = !renderer.vsync;
        SDL_GL_SetSwapInterval(renderer.vsync);
    }

    ImGui::NewLine(); ImGui::Separator();
    ImGui::Text("Voxel Cone Tracing");

    ImGui::DragFloat("Range", &renderer.voxelize->worldSize, 0.05f, 1.0f, FLT_MAX, "%.2f");

    ImGui::NewLine(); ImGui::Separator();

    ImGui::Text("Shadow Mapping");
    ImGui::Separator();

    if (ImGui::DragFloat("Bias constant", &renderer.shadows->settings.depthBiasConstant, 0.01f, 0.0f, FLT_MAX, "%.2f")) {}
    if (ImGui::DragFloat("Bias slope factor", &renderer.shadows->settings.depthBiasSlope, 0.01f, 0.0f, FLT_MAX, "%.2f")) {}
    if (ImGui::DragFloat("Cascade lambda", &renderer.shadows->settings.cascadeSplitLambda, 0.0001f, 0.0f, 1.0f, "%.4f")) {
        renderer.shadows->updatePerspectiveConstants(editor->getViewport());
    }

    ImGui::DragFloat3("Bloom threshold", glm::value_ptr(renderer.shading->settings.bloomThreshold), 0.01f, 0.0f, 10.0f, "%.3f");

    float fov = editor->getViewport().getCamera().getFOV();
    if (ImGui::DragFloat("Camera FOV", &fov, 1.0f, 5.0f, 105.0f, "%.2f", 1.0f)) {
    }

    ImGui::End();
}

} // raekor

