#include "pch.h"
#include "assetsWidget.h"

#include "iter.h"
#include "scene.h"
#include "components.h"
#include "application.h"
#include "IconsFontAwesome5.h"

namespace Raekor {

RTTI_DEFINE_TYPE_NO_FACTORY(ComponentsWidget) {}

ComponentsWidget::ComponentsWidget(Application* inApp) : IWidget(inApp, reinterpret_cast<const char*>( ICON_FA_PALETTE "  Components " )) {}

void ComponentsWidget::Draw(Widgets* inWidgets, float dt)
{
	ImGui::Begin(m_Title.c_str(), &m_Open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	m_Visible = ImGui::IsWindowAppearing();

	auto& scene = GetScene();

	constexpr std::array component_type_names = { "Material", "Animation" };

	ImGui::SetNextItemWidth(ImGui::CalcTextSize(component_type_names[m_ComponentIndex]).x * 2.f);

	if (ImGui::BeginCombo("##ComponentsWidgetComponentType", component_type_names[m_ComponentIndex]))
	{
		for (int i = 0; i < component_type_names.size(); i++)
		{
			if (ImGui::Selectable(component_type_names[i], i == m_ComponentIndex))
				m_ComponentIndex = i;
		}

		ImGui::EndCombo();
	}

	if (ImGui::BeginTable("Assets", 24, ImGuiTableFlags_ScrollY))
	{
		int component_index = 0;

		gForEachType<Material, Animation>([&](auto c)
		{
			using T = decltype( c );

			if (component_index == m_ComponentIndex)
			{
				for (const auto& [entity, component, name] : GetScene().Each<T, Name>())
				{
					ImGui::TableNextColumn();

					if (m_Editor->GetActiveEntity() == entity)
						ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonHovered));

					ImGui::PushID(uint32_t(entity));

					const bool clicked = DrawClickableComponent(entity, component);

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

				if (ImGui::BeginPopupContextWindow())
				{
					if (ImGui::MenuItem("Create.."))
					{
						Entity entity = scene.Create();
						scene.Add<Name>(entity).name = "New";

						if constexpr (std::is_same_v<T, Material>)
						{
							scene.Add<T>(entity, Material::Default);
						}
						else
							scene.Add<T>(entity);

						m_Editor->SetActiveEntity(entity);
					}

					ImGui::EndPopup();
				}
			}

			component_index++;
		});

		ImGui::EndTable();
	}

	ImGui::End();
}


bool ComponentsWidget::DrawClickableComponent(Entity inEntity, const Material& inMaterial)
{
	auto clicked = false;

	if (inMaterial.gpuAlbedoMap && !inMaterial.albedoFile.empty())
	{
		clicked = ImGui::ImageButton("##ComponentsWidget::Draw",
			(void*)( (intptr_t)m_Editor->GetRenderInterface()->GetImGuiTextureID(inMaterial.gpuAlbedoMap) ),
			ImVec2(64 * ImGui::GetWindowDpiScale(), 64 * ImGui::GetWindowDpiScale()),
			ImVec2(0, 0), ImVec2(1, 1),
			ImVec4(0, 1, 0, 1)
		);
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(inMaterial.albedo.r, inMaterial.albedo.g, inMaterial.albedo.b, inMaterial.albedo.a));

		clicked = ImGui::Button(
			std::string("##" + GetScene().Get<Name>(inEntity).name).c_str(),
			ImVec2(64 * ImGui::GetWindowDpiScale() + ImGui::GetStyle().FramePadding.x * 2, 64 * ImGui::GetWindowDpiScale() + ImGui::GetStyle().FramePadding.y * 2)
		);
		ImGui::PopStyleColor();
	}

	return clicked;
}

bool ComponentsWidget::DrawClickableComponent(Entity inEntity, const Animation& inAnimation)
{
	return ImGui::Button
	(
		reinterpret_cast<const char*>( ICON_FA_WALKING "##ComponentsWidget::Draw Graph" ), 
		ImVec2(64 * ImGui::GetWindowDpiScale() + ImGui::GetStyle().FramePadding.x * 2, 64 * ImGui::GetWindowDpiScale() + ImGui::GetStyle().FramePadding.y * 2)
	);
}

} // raekor