#include "pch.h"
#include "randomWidget.h"
#include "editor.h"
#include "renderer.h"

namespace Raekor {

RandomWidget::RandomWidget(Editor* editor) : 
    IWidget(editor),  
    renderer(editor->renderer)
{}



void RandomWidget::draw() {
    ImGui::Begin("Random");
    ImGui::SetItemDefaultFocus();

    if (ImGui::RadioButton("Vsync", renderer.vsync)) {
        renderer.vsync = !renderer.vsync;
        SDL_GL_SetSwapInterval(renderer.vsync);
    }

    ImGui::NewLine(); ImGui::Separator();
    ImGui::Text("Voxel Cone Tracing");

    ImGui::DragFloat("Range", &renderer.voxelizePass->worldSize, 0.05f, 1.0f, FLT_MAX, "%.2f");

    ImGui::NewLine(); ImGui::Separator();

    ImGui::Text("Shadow Mapping");
    ImGui::Separator();

    if (ImGui::DragFloat2("Planes", glm::value_ptr(renderer.shadowMapPass->settings.planes), 0.1f)) {}
    if (ImGui::DragFloat("Size", &renderer.shadowMapPass->settings.size)) {}
    if (ImGui::DragFloat("Bias constant", &renderer.shadowMapPass->settings.depthBiasConstant, 0.01f, 0.0f, FLT_MAX, "%.2f")) {}
    if (ImGui::DragFloat("Bias slope factor", &renderer.shadowMapPass->settings.depthBiasSlope, 0.01f, 0.0f, FLT_MAX, "%.2f")) {}

    ImGui::End();
}

} // raekor

