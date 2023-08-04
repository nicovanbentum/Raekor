#include "pch.h"
#include "assetsWidget.h"
#include "components.h"
#include "application.h"
#include "IconsFontAwesome5.h"

namespace Raekor {

RTTI_CLASS_CPP_NO_FACTORY(AssetsWidget) {}

AssetsWidget::AssetsWidget(Application* inApp) : IWidget(inApp, reinterpret_cast<const char*>(ICON_FA_PALETTE "  Materials ")) {}

void AssetsWidget::Draw(Widgets* inWidgets, float dt) {
    ImGui::Begin(m_Title.c_str(), &m_Open);
    m_Visible = ImGui::IsWindowAppearing();

    auto materials = IWidget::GetScene().view<Material, Name>();

    auto& style = ImGui::GetStyle();

    if (ImGui::BeginTable("Assets", 24)) {
        for (auto entity : materials) {
            auto [material, name] = materials.get<Material, Name>(entity);

            ImGui::TableNextColumn();

            if (m_Editor->GetActiveEntity() == entity)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.2f)));
            
            bool clicked = false;
            ImGui::PushID(entt::to_integral(entity));

            if (material.gpuAlbedoMap) {
                clicked = ImGui::ImageButton(
                    (void*)((intptr_t)m_Editor->GetRenderer()->GetImGuiTextureID(material.gpuAlbedoMap)),
                    ImVec2(64 * ImGui::GetWindowDpiScale(), 64 * ImGui::GetWindowDpiScale()), 
                    ImVec2(0, 0), ImVec2(1, 1), -1, 
                    ImVec4(0, 1, 0, 1)
                );
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec(material.albedo));

                clicked = ImGui::Button(
                    std::string("##" + name.name).c_str(), 
                    ImVec2(64 * ImGui::GetWindowDpiScale() + style.FramePadding.x * 2, 64 * ImGui::GetWindowDpiScale() + style.FramePadding.y * 2)
                );
                ImGui::PopStyleColor();
            }


            if (clicked)
                m_Editor->SetActiveEntity(entity);

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

            ImGui::Text(name.name.c_str());
        }

        ImGui::EndTable();
    }


    ImGui::End();
}

} // raekor