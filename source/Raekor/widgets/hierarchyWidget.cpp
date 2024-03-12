#include "pch.h"
#include "hierarchyWidget.h"
#include "application.h"
#include "components.h"
#include "scene.h"
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

	ImGui::PushItemWidth(-1);
	ImGui::InputText("##Filter", &m_Filter);
	ImGui::PopItemWidth();

	ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_TabHovered));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetColorU32(ImGuiCol_TabHovered));

	Scene::TraverseFunction Traverse = [](void* inContext, Scene& inScene, Entity inEntity) 
	{
		HierarchyWidget* widget = (HierarchyWidget*)inContext;

		if (inScene.GetParent(inEntity) == inScene.GetRootEntity())
		{
			if (inScene.HasChildren(inEntity))
			{
				if (widget->DrawFamilyNode(inScene, inEntity))
				{

					widget->DrawFamily(inScene, inEntity);
					ImGui::TreePop();
				}
			}
			else
				widget->DrawChildlessNode(inScene, inEntity);
		}
	};

	GetScene().TraverseDepthFirst(GetScene().GetRootEntity(), Traverse, this);

	ImGui::PopStyleColor(2);

	DropTargetWindow(GetScene());

	ImGui::End();
}


bool HierarchyWidget::DrawFamilyNode(Scene& inScene, Entity inEntity)
{
	const Name& name = inScene.Get<Name>(inEntity);
	const Entity active = m_Editor->GetActiveEntity();

	const auto  selected = active == inEntity ? ImGuiTreeNodeFlags_Selected : 0;
	const auto  tree_flags = selected | ImGuiTreeNodeFlags_OpenOnArrow;

	const float font_size = ImGui::GetFontSize();
	if (ImGui::Selectable((const char*)ICON_FA_CUBE "   ", active == inEntity, ImGuiSelectableFlags_None, ImVec2(font_size, font_size))) {}

	ImGui::SameLine();
	const bool opened = ImGui::TreeNodeEx(name.name.c_str(), tree_flags);

	if (ImGui::BeginPopupContextItem())
	{
		m_Editor->SetActiveEntity(inEntity);

		if (ImGui::MenuItem("Delete"))
		{
			GetScene().DestroySpatialEntity(inEntity);
			m_Editor->SetActiveEntity(Entity::Null);
		}

		if (ImGui::MenuItem("Select Children"))
		{

		}

		ImGui::EndPopup();
	}

	if (ImGui::IsItemClicked())
		m_Editor->SetActiveEntity(active == inEntity ? Entity::Null : inEntity);

	DropTargetNode(inScene, inEntity);

	return opened;
}


void HierarchyWidget::DrawChildlessNode(Scene& inScene, Entity inEntity)
{
	auto& name = inScene.Get<Name>(inEntity);
	ImGui::PushID(uint32_t(inEntity));

	Entity active_entity = m_Editor->GetActiveEntity();

	const auto font_size = ImGui::GetFontSize();
	if (ImGui::Selectable((const char*)ICON_FA_CUBE "   ", active_entity == inEntity, ImGuiSelectableFlags_None, ImVec2(font_size, font_size))) {}

	ImGui::SameLine();

	if (ImGui::Selectable(name.name.c_str(), inEntity == active_entity))
		m_Editor->SetActiveEntity(active_entity == inEntity ? Entity::Null : inEntity);

	if (ImGui::BeginPopupContextItem())
	{
		m_Editor->SetActiveEntity(inEntity);

		if (ImGui::MenuItem("Delete"))
		{
			GetScene().DestroySpatialEntity(inEntity);
			m_Editor->SetActiveEntity(Entity::Null);
		}

		ImGui::EndPopup();
	}

	ImGui::PopID();

	DropTargetNode(inScene, inEntity);
}


void HierarchyWidget::DropTargetNode(Scene& inScene, Entity inEntity)
{
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover | ImGuiDragDropFlags_SourceNoHoldToOpenOthers))
	{
		ImGui::SetDragDropPayload("drag_drop_entity", &inEntity, sizeof(Entity));
		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_entity"))
		{
			Entity child = *reinterpret_cast<const Entity*>( payload->Data );

			inScene.ParentTo(child, inEntity);
		}

		ImGui::EndDragDropTarget();
	}
}


void HierarchyWidget::DropTargetWindow(Scene& inScene)
{
	if (ImGui::BeginDragDropTargetCustom(ImGui::GetCurrentWindow()->InnerRect, ImGui::GetCurrentWindow()->ID))
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_entity"))
		{
			Entity entity = *reinterpret_cast<const Entity*>( payload->Data );
			Transform& transform = inScene.Get<Transform>(entity);

			transform.localTransform = transform.worldTransform;
			transform.Decompose();

			inScene.ParentTo(entity, inScene.GetRootEntity());
		}

		ImGui::EndDragDropTarget();
	}
}


void HierarchyWidget::DrawFamily(Scene& inScene, Entity inEntity)
{
	if (inScene.HasChildren(inEntity))
	{
		for (Entity child : inScene.GetChildren(inEntity))
		{
			if (inScene.HasChildren(child))
			{
				if (DrawFamilyNode(inScene, child))
				{
					DrawFamily(inScene, child);
					ImGui::TreePop();
				}
			}
			else
				DrawChildlessNode(inScene, child);
		}
	}
}

} // raekor