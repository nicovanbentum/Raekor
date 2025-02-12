#include "pch.h"
#include "HierarchyWidget.h"
#include "Application.h"
#include "Components.h"
#include "Scene.h"
#include "Editor.h"
#include "Input.h"

namespace RK {

RTTI_DEFINE_TYPE_NO_FACTORY(HierarchyWidget) {}

HierarchyWidget::HierarchyWidget(Editor* inEditor) :
	IWidget(inEditor, reinterpret_cast<const char*>( ICON_FA_STREAM " Scene " ))
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

	ImGuiSelectionBasicStorage& multi_select = m_Editor->GetMultiSelect();

	ImGuiMultiSelectIO* ms_io = ImGui::BeginMultiSelect(ImGuiMultiSelectFlags_ScopeWindow | ImGuiMultiSelectFlags_BoxSelect1d | ImGuiMultiSelectFlags_NoRangeSelect | ImGuiMultiSelectFlags_ClearOnClickVoid, multi_select.Size, 100);

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

	ms_io = ImGui::EndMultiSelect();

	if (ms_io->Requests.size())
		multi_select.ApplyRequests(ms_io);

	if (multi_select.Size > 1)
		SetActiveEntity(Entity::Null);

	ImGui::PopStyleColor(2);

	DropTargetWindow(GetScene());

	ImGui::End();
}


bool HierarchyWidget::DrawFamilyNode(Scene& inScene, Entity inEntity)
{
	ImGuiSelectionBasicStorage& multi_select = m_Editor->GetMultiSelect();

	const String name = inScene.Has<Name>(inEntity) ? inScene.Get<Name>(inEntity).name : "N/A";
	const Entity active = m_Editor->GetActiveEntity();
	const bool is_selected = active == inEntity || multi_select.Contains(inEntity);

	const ImGuiTreeNodeFlags tree_selected = is_selected ? ImGuiTreeNodeFlags_Selected : 0;
	const ImGuiTreeNodeFlags tree_flags = tree_selected | ImGuiTreeNodeFlags_OpenOnArrow;
	const float font_size = ImGui::GetFontSize();

	//ImGui::Selectable((const char*)ICON_FA_CUBE "   ", is_selected, ImGuiSelectableFlags_None, ImVec2(font_size, font_size));

	//ImGui::SameLine();
	
	ImGui::SetNextItemSelectionUserData(inEntity);

	const bool opened = ImGui::TreeNodeEx(name.c_str(), tree_flags);

	if (ImGui::BeginPopupContextItem())
	{
		m_Editor->SetActiveEntity(inEntity);

		if (ImGui::MenuItem("Delete"))
		{
			multi_select.Clear();
			m_Editor->SetActiveEntity(Entity::Null);
			GetScene().DestroySpatialEntity(inEntity);
		}

		if (ImGui::MenuItem("Select Children"))
		{
		}

		ImGui::EndPopup();
	}

	if (ImGui::IsItemClicked())
	{
		multi_select.Clear();
		m_Editor->SetActiveEntity(active == inEntity ? Entity::Null : inEntity);
	}

	DropTargetNode(inScene, inEntity);

	return opened;
}


void HierarchyWidget::DrawChildlessNode(Scene& inScene, Entity inEntity)
{
	ImGuiSelectionBasicStorage& multi_select = m_Editor->GetMultiSelect();

	ImGui::PushID(uint32_t(inEntity));

	Entity active_entity = m_Editor->GetActiveEntity();
	const float font_size = ImGui::GetFontSize();
	const bool is_selected = active_entity == inEntity || multi_select.Contains(inEntity);

	//if (ImGui::Selectable((const char*)ICON_FA_CUBE "   ", is_selected, ImGuiSelectableFlags_None, ImVec2(font_size, font_size))) {}

	//ImGui::SameLine();

	String name = inScene.Has<Name>(inEntity) ? inScene.Get<Name>(inEntity).name : "N/A";

	ImGui::SetNextItemSelectionUserData(inEntity);

	const ImGuiTreeNodeFlags tree_selected = is_selected ? ImGuiTreeNodeFlags_Selected : 0;
	const ImGuiTreeNodeFlags tree_flags = tree_selected | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

	const bool opened = ImGui::TreeNodeEx(name.c_str(), tree_flags);
	ImGui::TreePop();
	//if (ImGui::Selectable(name.c_str(), inEntity == active_entity || multi_select.Contains(inEntity)))
		//m_Editor->SetActiveEntity(active_entity == inEntity ? Entity::Null : inEntity);

	if (ImGui::BeginPopupContextItem())
	{
		m_Editor->SetActiveEntity(inEntity);

		if (ImGui::MenuItem("Delete"))
		{
			void* it = NULL; ImGuiID id; 
			while (multi_select.GetNextSelectedItem(&it, &id))
			{ 
				m_Editor->SetActiveEntity(Entity::Null);
				GetScene().DestroySpatialEntity(Entity(id));
			}

			multi_select.Clear();
		}

        if (multi_select.Size < 2)
        {
            if (ImGui::MenuItem("Create Child"))
            {
                Entity child_entity = GetScene().CreateSpatialEntity("Child");
                GetScene().ParentTo(child_entity, inEntity);
            }
        }

		ImGui::EndPopup();
	}

	if (ImGui::IsItemClicked())
	{
		multi_select.Clear();
		m_Editor->SetActiveEntity(active_entity == inEntity ? Entity::Null : inEntity);
	}

	ImGui::PopID();

	DropTargetNode(inScene, inEntity);
}


void HierarchyWidget::DropTargetNode(Scene& inScene, Entity inEntity)
{
	ImGuiSelectionBasicStorage& multi_select = m_Editor->GetMultiSelect();

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

			if (inScene.Has<Transform>(child))
			{
				inScene.ParentTo(child, inEntity);
			}
		}

		ImGui::EndDragDropTarget();
	}
}


void HierarchyWidget::DropTargetWindow(Scene& inScene)
{
	ImGuiSelectionBasicStorage& multi_select = m_Editor->GetMultiSelect();

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
	ImGuiSelectionBasicStorage& multi_select = m_Editor->GetMultiSelect();

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


void HierarchyWidget::OnEvent(Widgets* inWidgets, const SDL_Event& inEvent) 
{
	ImGuiSelectionBasicStorage& multi_select = m_Editor->GetMultiSelect();

	if (inEvent.type == SDL_KEYDOWN && !inEvent.key.repeat && !g_Input->IsRelativeMouseMode())
	{
		switch (inEvent.key.keysym.sym)
		{
			case SDLK_DELETE:
			{
				void* it = NULL; ImGuiID id;
				while (multi_select.GetNextSelectedItem(&it, &id))
				{
					GetScene().DestroySpatialEntity(Entity(id));
					m_Editor->SetActiveEntity(Entity::Null);
				}
			} break;
		}
	}
}


} // raekor