#include "pch.h"
#include "assetsWidget.h"
#include "editor.h"

#include "IconsFontAwesome5.h"

namespace Raekor {

AssetsWidget::AssetsWidget(Editor* editor) : IWidget(editor, "Asset Browser") {}

void AssetsWidget::draw(float dt) {
    ImGui::Begin(title.c_str());

    auto materialView = IWidget::scene().view<Material, Name>();

    auto& style = ImGui::GetStyle();

    if (ImGui::BeginTable("Assets", 24)) {
        for (auto entity : materialView) {
            auto& [material, name] = materialView.get<Material, Name>(entity);
            std::string selectableName = name.name.substr(0, 9).c_str() + std::string("...");

            ImGui::TableNextColumn();

            if (editor->active == entity) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.2f)));
            }
            
            bool clicked = false;

            if (material.albedo) {
                clicked = ImGui::ImageButton(
                    (void*)((intptr_t)material.albedo),
                    ImVec2(64 * ImGui::GetWindowDpiScale(), 64 * ImGui::GetWindowDpiScale()), 
                    ImVec2(0, 0), ImVec2(1, 1), -1, 
                    ImVec4(0, 1, 0, 1)
                );
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec(material.baseColour));
                clicked = ImGui::Button(
                    std::string("##" + name.name).c_str(), 
                    ImVec2(64 * ImGui::GetWindowDpiScale() + style.FramePadding.x * 2, 64 * ImGui::GetWindowDpiScale() + style.FramePadding.y * 2)
                );
                ImGui::PopStyleColor();
            }

            ImGui::PushID(entt::to_integral(entity));

            if (clicked) {
                editor->active = entity;
            }

            //if (ImGui::Button(ICON_FA_ARCHIVE, ImVec2(64 * ImGui::GetWindowDpiScale(), 64 * ImGui::GetWindowDpiScale()))) {
            //    editor->active = entity;
            //}

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