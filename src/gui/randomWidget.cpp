#include "pch.h"
#include "randomWidget.h"
#include "editor.h"
#include "renderer.h"

namespace Raekor {

RandomWidget::RandomWidget(Editor* editor) : 
    IWidget(editor, "Random"),  
    renderer(editor->renderer)
{}



void RandomWidget::draw() {
    ImGui::Begin(title.c_str());
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

    if (ImGui::DragFloat("Bias constant", &renderer.shadowMapPass->settings.depthBiasConstant, 0.01f, 0.0f, FLT_MAX, "%.2f")) {}
    if (ImGui::DragFloat("Bias slope factor", &renderer.shadowMapPass->settings.depthBiasSlope, 0.01f, 0.0f, FLT_MAX, "%.2f")) {}
    if (ImGui::DragFloat("Cascade lambda", &renderer.shadowMapPass->settings.cascadeSplitLambda, 0.0001f, 0.0f, 1.0f, "%.4f")) {}

    ImGui::End();
}

} // raekor

