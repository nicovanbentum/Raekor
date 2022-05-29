#include "pch.h"
#include "randomWidget.h"
#include "editor.h"
#include "renderer.h"
#include "Raekor/systems.h"

namespace Raekor {

RandomWidget::RandomWidget(Editor* editor) :
    IWidget(editor, "Random")
{}

void RandomWidget::draw(float dt) {
    auto& renderer = IWidget::GetRenderer();

    ImGui::Begin(title.c_str());
    ImGui::SetItemDefaultFocus();

    if (ImGui::Button("Optimize Scene Graph")) {
        GetActiveEntity() = sInvalidEntity;
        NodeSystem::sOptimize(GetScene());
    }

    if (ImGui::Checkbox("VSync", (bool*)(&renderer.settings.vsync))) {
        SDL_GL_SetSwapInterval(renderer.settings.vsync);
    }

    ImGui::SameLine();

    if (ImGui::Checkbox("TAA", (bool*)(&renderer.settings.enableTAA))) {
        renderer.frameNr = 0;
    }

    ImGui::NewLine(); ImGui::Separator();


    ImGui::Text("VCTGI");

    ImGui::DragFloat("Radius", &renderer.voxelize->worldSize, 0.05f, 1.0f, FLT_MAX, "%.2f");

    ImGui::NewLine(); ImGui::Separator();

    ImGui::Text("CSM");

    if (ImGui::DragFloat("Bias constant", &renderer.shadowMaps->settings.depthBiasConstant, 0.01f, 0.0f, FLT_MAX, "%.2f")) {}
    if (ImGui::DragFloat("Bias slope factor", &renderer.shadowMaps->settings.depthBiasSlope, 0.01f, 0.0f, FLT_MAX, "%.2f")) {}
    if (ImGui::DragFloat("Split lambda", &renderer.shadowMaps->settings.cascadeSplitLambda, 0.0001f, 0.0f, 1.0f, "%.4f")) {
        renderer.shadowMaps->updatePerspectiveConstants(editor->GetViewport());
    }

    ImGui::NewLine(); ImGui::Separator();

    ImGui::Text("Bloom");
    ImGui::DragFloat3("Threshold", glm::value_ptr(renderer.deferShading->settings.bloomThreshold), 0.01f, 0.0f, 10.0f, "%.3f");

    ImGui::End();
}

} // raekor

