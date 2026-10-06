#include "pch.h"
#include "HierarchyWidget.h"
#include "Application.h"
#include "Components.h"
#include "Primitives.h"
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
	m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

	Scene& scene = GetScene();

	DrawToolbar(scene);

	ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 1.1f);

	if (ImGui::BeginChild("##HierarchyTree"))
	{
		if (m_Filter.empty())
		{
			for (Entity entity : scene.GetChildren(scene.GetRootEntity()))
				DrawNode(scene, entity);
		}
		else
			DrawFilteredList(scene);

		if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			m_Editor->GetMultiSelect().Clear();
			SetActiveEntity(Entity::Null);
		}

		if (ImGui::BeginPopupContextWindow("##HierarchyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			DrawCreateMenuItems(scene, Entity::Null);
			ImGui::EndPopup();
		}

		DropTargetWindow(scene);
	}

	ImGui::EndChild();

	ImGui::PopStyleVar();

	m_PrevActiveEntity = GetActiveEntity();

	ImGui::End();
}


void HierarchyWidget::DrawToolbar(Scene& inScene)
{
	const float add_button_width = ImGui::GetFrameHeight();

	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - add_button_width - ImGui::GetStyle().ItemSpacing.x);
	ImGui::InputTextWithHint("##Filter", (const char*)ICON_FA_SEARCH "  Search entities..", &m_Filter);

	ImGui::SameLine();

	if (ImGui::Button((const char*)ICON_FA_PLUS, ImVec2(add_button_width, 0.0f)))
		ImGui::OpenPopup("##HierarchyAdd");

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Create a new entity");

	if (ImGui::BeginPopup("##HierarchyAdd"))
	{
		DrawCreateMenuItems(inScene, Entity::Null);
		ImGui::EndPopup();
	}
}


void HierarchyWidget::DrawNode(Scene& inScene, Entity inEntity)
{
	const bool has_children = inScene.HasChildren(inEntity);
	const Name* name = inScene.GetPtr<Name>(inEntity);
	const Entity active_entity = GetActiveEntity();

	ImGui::PushID(uint32_t(inEntity));

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

	if (!has_children)
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

	if (IsSelected(inEntity))
		flags |= ImGuiTreeNodeFlags_Selected;

	if (active_entity != m_PrevActiveEntity && active_entity != Entity::Null && IsDescendantOf(inScene, active_entity, inEntity))
		ImGui::SetNextItemOpen(true);

	bool opened = false;

	if (m_RenameEntity == inEntity)
	{
		opened = ImGui::TreeNodeEx("##RenameNode", flags | ImGuiTreeNodeFlags_AllowOverlap, "%s", GetEntityIcon(inScene, inEntity));
		ImGui::SameLine();

		if (m_FocusRename)
		{
			ImGui::SetKeyboardFocusHere();
			m_FocusRename = false;
		}

		ImGui::SetNextItemWidth(-FLT_MIN);

		if (ImGui::InputText("##Rename", &m_RenameBuffer, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
		{
			if (Name* rename = inScene.GetPtr<Name>(inEntity))
			{
				rename->name = m_RenameBuffer;
				m_Editor->MarkSceneChanged();
			}

			m_RenameEntity = Entity::Null;
		}
		else if (ImGui::IsItemDeactivated())
			m_RenameEntity = Entity::Null;
	}
	else
	{
		opened = ImGui::TreeNodeEx("##Node", flags, "%s  %s", GetEntityIcon(inScene, inEntity), name ? name->name.c_str() : "Unnamed");

		if (active_entity == inEntity && active_entity != m_PrevActiveEntity && !ImGui::IsItemVisible())
			ImGui::SetScrollHereY();

		HandleSelection(inEntity);
		HandleDragDrop(inScene, inEntity);

		if (ImGui::BeginPopupContextItem("##NodeContext"))
		{
			DrawContextMenu(inScene, inEntity);
			ImGui::EndPopup();
		}
	}

	if (opened && has_children)
	{
		for (Entity child : inScene.GetChildren(inEntity))
			DrawNode(inScene, child);

		ImGui::TreePop();
	}

	ImGui::PopID();
}


void HierarchyWidget::DrawFilteredList(Scene& inScene)
{
	const ImGuiTextFilter filter = ImGuiTextFilter(m_Filter.c_str());

	uint32_t match_count = 0;

	for (const auto& [entity, name] : inScene.Each<Name>())
	{
		if (!inScene.Has<Transform>(entity) || !filter.PassFilter(name.name.c_str()))
			continue;

		match_count++;

		ImGui::PushID(uint32_t(entity));

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

		if (IsSelected(entity))
			flags |= ImGuiTreeNodeFlags_Selected;

		ImGui::TreeNodeEx("##FilteredNode", flags, "%s  %s", GetEntityIcon(inScene, entity), name.name.c_str());

		HandleSelection(entity);
		HandleDragDrop(inScene, entity);

		if (ImGui::BeginPopupContextItem("##NodeContext"))
		{
			DrawContextMenu(inScene, entity);
			ImGui::EndPopup();
		}

		const Entity parent = inScene.GetParent(entity);

		if (parent != Entity::Null && parent != inScene.GetRootEntity())
		{
			if (const Name* parent_name = inScene.GetPtr<Name>(parent))
			{
				ImGui::SameLine();
				ImGui::TextDisabled("in %s", parent_name->name.c_str());
			}
		}

		ImGui::PopID();
	}

	if (match_count == 0)
		ImGui::TextDisabled("No entities match \"%s\"", m_Filter.c_str());
}


void HierarchyWidget::DrawContextMenu(Scene& inScene, Entity inEntity)
{
	if (!IsSelected(inEntity))
	{
		m_Editor->GetMultiSelect().Clear();
		SetActiveEntity(inEntity);
	}

	const bool is_editing = m_Editor->GetGameState() != GAME_RUNNING;

	ImGui::BeginDisabled(!is_editing);

	if (ImGui::MenuItem("Rename", "F2"))
	{
		const Name* name = inScene.GetPtr<Name>(inEntity);
		m_RenameBuffer = name ? name->name : "";
		m_RenameEntity = inEntity;
		m_FocusRename = true;
	}

	if (ImGui::MenuItem((const char*)ICON_FA_CLONE "  Duplicate", "Ctrl+D"))
		m_Editor->DuplicateSelection();

	if (ImGui::BeginMenu((const char*)ICON_FA_PLUS "  Create Child"))
	{
		DrawCreateMenuItems(inScene, inEntity);
		ImGui::EndMenu();
	}

	const Entity parent = inScene.GetParent(inEntity);

	if (ImGui::MenuItem("Move to Root", nullptr, false, parent != Entity::Null && parent != inScene.GetRootEntity()))
		Reparent(inScene, inEntity, inScene.GetRootEntity());

	if (ImGui::MenuItem("Frame in Viewport", nullptr, false, inScene.Has<Transform>(inEntity) && m_Editor->GetCameraEntity() == Entity::Null))
	{
		const Transform& transform = inScene.Get<Transform>(inEntity);

		Vec3 target = transform.GetPositionWorldSpace();
		float radius = 1.0f;

		if (const Mesh* mesh = inScene.GetPtr<Mesh>(inEntity))
		{
			const BBox3D bounds = mesh->bbox.Transformed(transform.worldTransform);
			target = bounds.GetCenter();
			radius = glm::max(glm::length(bounds.GetExtents()) * 0.5f, 0.1f);
		}

		Camera& camera = m_Editor->GetCamera();
		const float distance = radius / glm::tan(glm::radians(camera.GetFov()) * 0.5f);

		camera.SetPosition(target - camera.GetForward() * distance);
		camera.LookAt(target);

		m_Editor->SetViewportChanged(true);
	}

	ImGui::Separator();

	if (ImGui::MenuItem((const char*)ICON_FA_TRASH "  Delete", "Delete"))
		m_Editor->DeleteSelection();

	ImGui::EndDisabled();
}


void HierarchyWidget::DrawCreateMenuItems(Scene& inScene, Entity inParent)
{
	auto FinishCreation = [&](Entity inEntity)
	{
		if (inParent != Entity::Null)
			inScene.ParentTo(inEntity, inParent);

		SetActiveEntity(inEntity);
		m_Editor->SetViewportChanged(true);
		m_Editor->MarkSceneChanged();
	};

	auto CreateShape = [&](const char* inName, auto inCreateFunction)
	{
		const Entity entity = inScene.CreateSpatialEntity(inName);

		Mesh& mesh = inScene.Add<Mesh>(entity);
		inCreateFunction(mesh);

		GetRenderInterface().UploadMeshBuffers(entity, mesh);
		FinishCreation(entity);
	};

	ImGui::BeginDisabled(m_Editor->GetGameState() == GAME_RUNNING);

	if (ImGui::MenuItem((const char*)ICON_FA_CUBE "  Empty Entity"))
		FinishCreation(inScene.CreateSpatialEntity("Empty"));

	if (ImGui::MenuItem("      Cube"))
		CreateShape("Cube", [](Mesh& ioMesh) { Mesh::CreateCube(ioMesh, 1.0f); });

	if (ImGui::MenuItem("      Sphere"))
		CreateShape("Sphere", [](Mesh& ioMesh) { Mesh::CreateSphere(ioMesh, 0.5f, 32, 32); });

	if (ImGui::MenuItem("      Plane"))
		CreateShape("Plane", [](Mesh& ioMesh) { Mesh::CreatePlane(ioMesh, 1.0f); });

	ImGui::Separator();

	if (ImGui::MenuItem((const char*)ICON_FA_LIGHTBULB "  Point Light"))
	{
		const Entity entity = inScene.CreateSpatialEntity("Point Light");
		inScene.Add<Light>(entity).type = LIGHT_TYPE_POINT;
		FinishCreation(entity);
	}

	if (ImGui::MenuItem((const char*)ICON_FA_LIGHTBULB "  Spot Light"))
	{
		const Entity entity = inScene.CreateSpatialEntity("Spot Light");
		inScene.Add<Light>(entity).type = LIGHT_TYPE_SPOT;
		FinishCreation(entity);
	}

	if (ImGui::MenuItem((const char*)ICON_FA_CAMERA "  Camera"))
	{
		const Entity entity = inScene.CreateSpatialEntity("Camera");
		inScene.Add<Camera>(entity).SetFar(1000.0f);
		FinishCreation(entity);
	}

	ImGui::EndDisabled();
}


void HierarchyWidget::HandleSelection(Entity inEntity)
{
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && m_Editor->GetGameState() != GAME_RUNNING)
	{
		const Name* name = GetScene().GetPtr<Name>(inEntity);
		m_RenameBuffer = name ? name->name : "";
		m_RenameEntity = inEntity;
		m_FocusRename = true;
		return;
	}

	if (!ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemToggledOpen())
		return;

	ImGuiSelectionBasicStorage& multi_select = m_Editor->GetMultiSelect();

	if (ImGui::GetIO().KeyCtrl)
	{
		const Entity active_entity = GetActiveEntity();

		if (active_entity != Entity::Null)
			multi_select.SetItemSelected(ImGuiID(active_entity), true);

		multi_select.SetItemSelected(ImGuiID(inEntity), !multi_select.Contains(ImGuiID(inEntity)));

		m_Editor->SetActiveEntity(Entity::Null);

		if (multi_select.Size == 1)
		{
			void* iterator = nullptr;
			ImGuiID id = 0;
			multi_select.GetNextSelectedItem(&iterator, &id);
			SetActiveEntity(Entity(id));
		}
	}
	else
	{
		multi_select.Clear();
		SetActiveEntity(inEntity);
	}
}


void HierarchyWidget::HandleDragDrop(Scene& inScene, Entity inEntity)
{
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoHoldToOpenOthers))
	{
		ImGui::SetDragDropPayload("drag_drop_entity", &inEntity, sizeof(Entity));

		if (const Name* name = inScene.GetPtr<Name>(inEntity))
			ImGui::Text("%s  %s", GetEntityIcon(inScene, inEntity), name->name.c_str());

		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget())
	{
		const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_entity", ImGuiDragDropFlags_AcceptPeekOnly);

		if (payload)
		{
			const Entity child = *reinterpret_cast<const Entity*>( payload->Data );
			const bool is_valid = child != inEntity && inScene.Has<Transform>(child) && !IsDescendantOf(inScene, inEntity, child);

			if (is_valid && ImGui::AcceptDragDropPayload("drag_drop_entity"))
				Reparent(inScene, child, inEntity);
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
			const Entity entity = *reinterpret_cast<const Entity*>( payload->Data );

			if (inScene.Has<Transform>(entity))
				Reparent(inScene, entity, inScene.GetRootEntity());
		}

		ImGui::EndDragDropTarget();
	}
}


bool HierarchyWidget::IsSelected(Entity inEntity) const
{
	return m_Editor->GetActiveEntity() == inEntity || m_Editor->GetMultiSelect().Contains(ImGuiID(inEntity));
}


bool HierarchyWidget::IsDescendantOf(Scene& inScene, Entity inEntity, Entity inAncestor) const
{
	for (Entity parent = inScene.GetParent(inEntity); parent != Entity::Null; parent = inScene.GetParent(parent))
	{
		if (parent == inAncestor)
			return true;
	}

	return false;
}


void HierarchyWidget::Reparent(Scene& inScene, Entity inEntity, Entity inParent)
{
	if (inScene.GetParent(inEntity) == inParent)
		return;

	Transform& transform = inScene.Get<Transform>(inEntity);

	Mat4x4 parent_world_transform = Mat4x4(1.0f);

	if (inParent != inScene.GetRootEntity())
	{
		if (const Transform* parent_transform = inScene.GetPtr<Transform>(inParent))
			parent_world_transform = parent_transform->worldTransform;
	}

	transform.localTransform = glm::inverse(parent_world_transform) * transform.worldTransform;
	transform.Decompose();

	inScene.Unparent(inEntity);
	inScene.ParentTo(inEntity, inParent);

	m_Editor->SetViewportChanged(true);
	m_Editor->MarkSceneChanged();
}


const char* HierarchyWidget::GetEntityIcon(Scene& inScene, Entity inEntity) const
{
	if (inScene.Has<DirectionalLight>(inEntity))
		return (const char*)ICON_FA_SUN;

	if (inScene.Has<Light>(inEntity))
		return (const char*)ICON_FA_LIGHTBULB;

	if (inScene.Has<Camera>(inEntity))
		return (const char*)ICON_FA_CAMERA;

	if (inScene.Has<DDGISceneSettings>(inEntity))
		return (const char*)ICON_FA_GLOBE;

	if (inScene.Has<Skeleton>(inEntity))
		return (const char*)ICON_FA_BONE;

	if (inScene.Has<Mesh>(inEntity))
		return (const char*)ICON_FA_CUBE;

	if (inScene.Has<NativeScript>(inEntity))
		return (const char*)ICON_FA_CODE;

	if (inScene.HasChildren(inEntity))
		return (const char*)ICON_FA_LAYER_GROUP;

	return (const char*)ICON_FA_CIRCLE;
}


void HierarchyWidget::OnEvent(Widgets* inWidgets, const SDL_Event& inEvent)
{
	if (inEvent.type == SDL_EVENT_KEY_DOWN && !inEvent.key.repeat && inEvent.key.key == SDLK_F2 && m_Focused)
	{
		const Entity active_entity = GetActiveEntity();

		if (active_entity != Entity::Null && GetScene().Has<Name>(active_entity))
		{
			m_RenameBuffer = GetScene().Get<Name>(active_entity).name;
			m_RenameEntity = active_entity;
			m_FocusRename = true;
		}
	}
}


} // raekor
