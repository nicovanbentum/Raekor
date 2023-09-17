#include "pch.h"
#include "assetsWidget.h"

#include "scene.h"
#include "components.h"
#include "application.h"
#include "IconsFontAwesome5.h"

namespace Raekor {

RTTI_DEFINE_TYPE_NO_FACTORY(AssetsWidget) {}

AssetsWidget::AssetsWidget(Application* inApp) : IWidget(inApp, reinterpret_cast<const char*>( ICON_FA_PALETTE "  Materials " )) {}

void AssetsWidget::Draw(Widgets* inWidgets, float dt)
{
	ImGui::Begin(m_Title.c_str(), &m_Open);
	m_Visible = ImGui::IsWindowAppearing();

	auto& style = ImGui::GetStyle();

	if (ImGui::BeginTable("Assets", 24))
	{
		for (auto [entity, material, name] : GetScene().Each<Material, Name>())
		{
			ImGui::TableNextColumn();

			if (m_Editor->GetActiveEntity() == entity)
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonHovered));

			auto clicked = false;
			ImGui::PushID(uint32_t(entity));

			if (material.gpuAlbedoMap && !material.albedoFile.empty())
			{
				clicked = ImGui::ImageButton("##AssetsWidget::Draw",
					(void*)( (intptr_t)m_Editor->GetRenderInterface()->GetImGuiTextureID(material.gpuAlbedoMap) ),
					ImVec2(64 * ImGui::GetWindowDpiScale(), 64 * ImGui::GetWindowDpiScale()),
					ImVec2(0, 0), ImVec2(1, 1),
					ImVec4(0, 1, 0, 1)
				);
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec(material.albedo));

				clicked = ImGui::Button(
					std::string("##" + name.name).c_str(),
					ImVec2(64 * ImGui::GetWindowDpiScale() + style.FramePadding.x * 2, 64 * ImGui::GetWindowDpiScale() + style.FramePadding.y * 2)
				);
				ImGui::PopStyleColor();
			}

			if (m_Editor->GetActiveEntity() == entity)
				ImGui::PopStyleColor();

			if (clicked)
				m_Editor->SetActiveEntity(entity);

			//if (ImGui::Button(ICON_FA_ARCHIVE, ImVec2(64 * ImGui::GetWindowDpiScale(), 64 * ImGui::GetWindowDpiScale()))) {
			//    editor->active = entity;
			//}

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover | ImGuiDragDropFlags_SourceNoHoldToOpenOthers))
			{
				ImGui::SetDragDropPayload("drag_drop_mesh_material", &entity, sizeof(Entity));
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