#include "pch.h"
#include "hierarchyWidget.h"
#include "application.h"
#include "systems.h"
#include "components.h"
#include "input.h"

namespace Raekor {

RTTI_DEFINE_TYPE_NO_FACTORY(HierarchyWidget) {}

HierarchyWidget::HierarchyWidget(Application* inApp) :
	IWidget(inApp, reinterpret_cast<const char*>( ICON_FA_STREAM " Scene " ))
{
}


void HierarchyWidget::Draw(Widgets* inWidgets, float inDeltaTime)
{
	ImGui::Begin(m_Title.c_str(), &m_Open);
	m_Visible = ImGui::IsWindowAppearing();

	auto& scene = GetScene();
	auto active_entity = m_Editor->GetActiveEntity();

	for (auto [entity, node] : scene.Each<Node>())
	{
		if (!node.IsRoot())
			continue;

		if (node.HasChildren())
		{
			if (DrawFamilyNode(scene, entity, active_entity))
			{
				DrawFamily(scene, entity, active_entity);
				ImGui::TreePop();
			}
		}
		else
			DrawChildlessNode(scene, entity, active_entity);
	}

	DropTargetWindow(scene);

	ImGui::End();
}


bool HierarchyWidget::DrawFamilyNode(Scene& inScene, Entity inEntity, Entity& inActive)
{
	const auto& name = inScene.Get<Name>(inEntity);
	const auto  selected = inActive == inEntity ? ImGuiTreeNodeFlags_Selected : 0;
	const auto  tree_flags = selected | ImGuiTreeNodeFlags_OpenOnArrow;

	const auto font_size = ImGui::GetFontSize();
	if (ImGui::Selectable((const char*)ICON_FA_CUBE "   ", inActive == inEntity, ImGuiSelectableFlags_None, ImVec2(font_size, font_size))) {}

	ImGui::SameLine();
	bool opened = ImGui::TreeNodeEx(name.name.c_str(), tree_flags);

	if (ImGui::IsItemClicked())
		m_Editor->SetActiveEntity(inActive == inEntity ? NULL_ENTITY : inEntity);

	DropTargetNode(inScene, inEntity);

	return opened;
}


void HierarchyWidget::DrawChildlessNode(Scene& inScene, Entity inEntity, Entity& inActive)
{
	auto& name = inScene.Get<Name>(inEntity);
	ImGui::PushID(uint32_t(inEntity));

	const auto font_size = ImGui::GetFontSize();
	if (ImGui::Selectable((const char*)ICON_FA_CUBE "   ", inActive == inEntity, ImGuiSelectableFlags_None, ImVec2(font_size, font_size))) {}

	ImGui::SameLine();

	if (ImGui::Selectable(name.name.c_str(), inEntity == inActive))
		m_Editor->SetActiveEntity(inActive == inEntity ? NULL_ENTITY : inEntity);

	ImGui::PopID();

	DropTargetNode(inScene, inEntity);
}


void HierarchyWidget::DropTargetNode(Scene& inScene, Entity inEntity)
{
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover | ImGuiDragDropFlags_SourceNoHoldToOpenOthers))
	{
		ImGui::SetDragDropPayload("drag_drop_hierarchy_entity", &inEntity, sizeof(Entity));
		ImGui::EndDragDropSource();
	}

	auto& node = inScene.Get<Node>(inEntity);

	if (ImGui::BeginDragDropTarget())
	{
		if (const auto payload = ImGui::AcceptDragDropPayload("drag_drop_hierarchy_entity"))
		{
			const auto child = *reinterpret_cast<const Entity*>( payload->Data );

			if (child != inEntity)
			{
				bool child_is_parent = false;

				for (auto parent = node.parent; parent != NULL_ENTITY; parent = inScene.Get<Node>(parent).parent)
				{
					if (child == parent)
					{
						child_is_parent = true;
						break;
					}
				}

				if (!child_is_parent)
				{
					NodeSystem::sRemove(inScene, inScene.Get<Node>(child));
					NodeSystem::sAppend(inScene, inEntity, node, child, inScene.Get<Node>(child));
				}
			}
		}

		ImGui::EndDragDropTarget();
	}
}


void HierarchyWidget::DropTargetWindow(Scene& inScene)
{
	if (ImGui::BeginDragDropTargetCustom(ImGui::GetCurrentWindow()->InnerRect, ImGui::GetCurrentWindow()->ID))
	{
		if (const auto payload = ImGui::AcceptDragDropPayload("drag_drop_hierarchy_entity"))
		{
			const auto entity = *reinterpret_cast<const Entity*>( payload->Data );
			auto& node = inScene.Get<Node>(entity);
			auto& transform = inScene.Get<Transform>(entity);

			transform.localTransform = transform.worldTransform;
			transform.Decompose();

			NodeSystem::sRemove(inScene, node);
			node.parent = NULL_ENTITY;
		}

		ImGui::EndDragDropTarget();
	}
}


void HierarchyWidget::DrawFamily(Scene& inScene, Entity inEntity, Entity& inActive)
{
	const auto& node = inScene.Get<Node>(inEntity);

	if (node.HasChildren())
	{
		for (auto it = node.firstChild; it != NULL_ENTITY; it = inScene.Get<Node>(it).nextSibling)
		{
			const auto& child = inScene.Get<Node>(it);

			if (child.HasChildren())
			{
				if (DrawFamilyNode(inScene, it, inActive))
				{
					DrawFamily(inScene, it, inActive);
					ImGui::TreePop();
				}
			}
			else
				DrawChildlessNode(inScene, it, inActive);
		}
	}
}

} // raekor