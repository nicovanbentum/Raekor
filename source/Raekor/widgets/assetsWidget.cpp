#include "pch.h"
#include "assetsWidget.h"

#include "scene.h"
#include "components.h"
#include "application.h"
#include "IconsFontAwesome5.h"

namespace Raekor {

RTTI_DEFINE_TYPE_NO_FACTORY(AssetsWidget) {}

AssetsWidget::AssetsWidget(Application* inApp) : IWidget(inApp, reinterpret_cast<const char*>( ICON_FA_PALETTE "  Components Browser " )) {}

void AssetsWidget::Draw(Widgets* inWidgets, float dt)
{
	ImGui::Begin(m_Title.c_str(), &m_Open);
	m_Visible = ImGui::IsWindowAppearing();

	auto& scene = GetScene();

	if (ImGui::BeginPopupContextWindow())
	{
		if (ImGui::MenuItem("Create Material"))
		{
			auto entity = scene.Create();
			scene.Add<Name>(entity).name = "Material";
			scene.Add<Material>(entity, Material::Default);
			m_Editor->SetActiveEntity(entity);
		}

		ImGui::EndPopup();
	}

	auto& style = ImGui::GetStyle();

	if (ImGui::BeginTable("Assets", 24))
	{
		DrawMaterials();

		DrawAnimations();

		ImGui::EndTable();
	}


	ImGui::End();
}


void AssetsWidget::DrawMaterials()
{
	for (const auto& [entity, material, name] : GetScene().Each<Material, Name>())
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
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(material.albedo.r, material.albedo.g, material.albedo.b, material.albedo.a));

			clicked = ImGui::Button(
				std::string("##" + name.name).c_str(),
				ImVec2(64 * ImGui::GetWindowDpiScale() + ImGui::GetStyle().FramePadding.x * 2, 64 * ImGui::GetWindowDpiScale() + ImGui::GetStyle().FramePadding.y * 2)
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
			ImGui::SetDragDropPayload("drag_drop_entity", &entity, sizeof(Entity));
			ImGui::EndDragDropSource();
		}

		ImGui::PopID();

		ImGui::Text(name.name.c_str());
	}
}


void AssetsWidget::DrawAnimations()
{
	for (const auto& [entity, animation, name] : GetScene().Each<Animation, Name>())
	{
		ImGui::TableNextColumn();

		if (m_Editor->GetActiveEntity() == entity)
			ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonHovered));

		ImGui::PushID(uint32_t(entity));

		const bool clicked = ImGui::Button
		(
			reinterpret_cast<const char*>( ICON_FA_WALKING "##AssetsWidget::Draw Graph" ), 
			ImVec2(64 * ImGui::GetWindowDpiScale() + ImGui::GetStyle().FramePadding.x * 2, 64 * ImGui::GetWindowDpiScale() + ImGui::GetStyle().FramePadding.y * 2)
		);

		if (m_Editor->GetActiveEntity() == entity)
			ImGui::PopStyleColor();

		if (clicked)
			m_Editor->SetActiveEntity(entity);

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover | ImGuiDragDropFlags_SourceNoHoldToOpenOthers))
		{
			ImGui::SetDragDropPayload("drag_drop_entity", &entity, sizeof(Entity));
			ImGui::EndDragDropSource();
		}

		ImGui::PopID();

		ImGui::Text(name.name.c_str());
	}
}

} // raekor