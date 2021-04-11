#include "pch.h"
#include "assetsWidget.h"
#include "editor.h"

namespace Raekor {

AssetsWidget::AssetsWidget(Editor* editor) : IWidget(editor) {}

void AssetsWidget::draw() {
    ImGui::Begin("Asset Browser");

    auto materialView = editor->scene->view<ecs::MaterialComponent, ecs::NameComponent>();

    if (ImGui::BeginTable("Assets", 24)) {
        for (auto entity : materialView) {
            auto& [material, name] = materialView.get<ecs::MaterialComponent, ecs::NameComponent>(entity);
            std::string selectableName = name.name.substr(0, 9).c_str() + std::string("...");

            ImGui::TableNextColumn();

            if (editor->active == entity) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.2)));
            }

            ImGui::PushID(entt::to_integral(entity));
            if (ImGui::ImageButton((void*)((intptr_t)material.albedo),
                ImVec2(64 * ImGui::GetWindowDpiScale(), 64 * ImGui::GetWindowDpiScale()), ImVec2(0, 0), ImVec2(1, 1), -1, ImVec4(0, 1, 0, 1))) {
                editor->active = entity;
            }

            ImGuiDragDropFlags src_flags = ImGuiDragDropFlags_SourceNoDisableHover;
            src_flags |= ImGuiDragDropFlags_SourceNoHoldToOpenOthers;
            if (ImGui::BeginDragDropSource(src_flags)) {
                ImGui::SetDragDropPayload("drag_drop_mesh_material", &entity, sizeof(entt::entity));
                ImGui::EndDragDropSource();
            }

            ImGui::PopID();

            ImGui::Text(selectableName.c_str());
        }

        ImGui::EndTable();
    }


    ImGui::End();
}

} // raekor