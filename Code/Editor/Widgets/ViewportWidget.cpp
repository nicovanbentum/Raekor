#include "pch.h"
#include "viewportWidget.h"

#include "OS.h"
#include "GUI.h"
#include "Iter.h"
#include "Scene.h"
#include "Timer.h"
#include "Input.h"
#include "Script.h"
#include "Editor.h"
#include "Physics.h"
#include "Components.h"
#include "Primitives.h"
#include "Application.h"

namespace RK {

RTTI_DEFINE_TYPE_NO_FACTORY(ViewportWidget) {}

ViewportWidget::ViewportWidget(Editor* inEditor) :
	IWidget(inEditor, reinterpret_cast<const char*>( ICON_FA_VIDEO " Viewport " ))
{
}


void ViewportWidget::Draw(Widgets* inWidgets, float inDeltaTime)
{
	m_Changed = false;

	Scene& scene = IWidget::GetScene();
	Viewport& viewport = m_Editor->GetViewport();

    static int& show_border = g_CVariables->Create("r_show_border", 1, IF_DEBUG_ELSE(true, false));
    static int& show_debug_fps = g_CVariables->Create("r_show_debug_fps", 1, IF_DEBUG_ELSE(true, false));
    static int& show_debug_text = g_CVariables->Create("r_show_debug_text", 1, IF_DEBUG_ELSE(true, false));
	static int& show_debug_icons = g_CVariables->Create("r_show_debug_icons", 1, IF_DEBUG_ELSE(true, false));

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

	const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar;

	ImGui::SetNextWindowSize(ImVec2(160, 90), ImGuiCond_FirstUseEver);
	m_Visible = ImGui::Begin(m_Title.c_str(), &m_Open, flags);

	ImGui::PopStyleVar();

	const ImVec2 pre_scene_cursor_pos = ImGui::GetCursorPos();

	// figure out if we need to resize the viewport
	ImVec2 size = ImGui::GetContentRegionAvail();
	size.x = glm::max(size.x, 160.0f);
	size.y = glm::max(size.y, 90.0f);

	if (viewport.GetDisplaySize().x != uint32_t(size.x) || viewport.GetDisplaySize().y != uint32_t(size.y))
	{
		viewport.SetRenderSize({ size.x, size.y });
		viewport.SetDisplaySize({ size.x, size.y });
	}

	m_Focused = ImGui::IsWindowFocused();

	m_DisplayTexture = m_Editor->GetRenderInterface()->GetDisplayTexture();

	const ImVec4 border_color = show_border && m_Editor->GetGameState() != GAME_STOPPED ?
		( m_Editor->GetGameState() == GAME_RUNNING ? cRunningColor : cPausedColor ) : ImVec4(0, 0, 0, 0);

	ImGui::Image((ImTextureID)((intptr_t)m_DisplayTexture), size, ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), border_color);

	m_IsMouseOver = ImGui::IsItemHovered();
	const ImVec2 viewport_min = ImGui::GetItemRectMin();
	const ImVec2 viewport_max = ImGui::GetItemRectMax();

	m_WindowPos = viewport_min;
	m_WindowSize = viewport_max - viewport_min;

	const ImVec2 mouse_in_image = ImGui::GetMousePos() - viewport_min;
	const UVec2 mouse_pixel = UVec2(
		uint32_t(glm::clamp(mouse_in_image.x, 0.0f, size.x - 1.0f) * viewport.GetRenderSize().x / size.x),
		uint32_t(glm::clamp(mouse_in_image.y, 0.0f, size.y - 1.0f) * viewport.GetRenderSize().y / size.y)
	);

	if (GetActiveEntity() != Entity::Null && scene.Has<Transform>(GetActiveEntity()) && m_IsGizmoEnabled)
	{
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(viewport_min.x, viewport_min.y, viewport_max.x - viewport_min.x, viewport_max.y - viewport_min.y);

		Mat4x4 local_to_world_transform = Mat4x4(1.0f);
		Mat4x4 world_to_local_transform = Mat4x4(1.0f);

		Entity parent = scene.GetParent(GetActiveEntity());

		if (parent != Entity::Null && parent != scene.GetRootEntity())
		{
			if (scene.Has<Transform>(parent))
			{
				const Transform& parent_transform = scene.Get<Transform>(parent);
				local_to_world_transform = parent_transform.worldTransform;
				world_to_local_transform = glm::inverse(parent_transform.worldTransform);
			}
		}

		Transform& transform = scene.Get<Transform>(GetActiveEntity());
		Mat4x4 world_space_transform = local_to_world_transform * transform.localTransform;

		Vec3 additional_translation = Vec3(0.0f);

		if (scene.Has<Mesh>(GetActiveEntity()))
		{
			const Mesh& mesh = scene.Get<Mesh>(GetActiveEntity());
			const BBox3D world_space_bounds = mesh.bbox.Transformed(transform.worldTransform);
			additional_translation = world_space_bounds.GetCenter() - transform.GetPositionWorldSpace();
		}

		world_space_transform = glm::translate(world_space_transform, additional_translation);

		ImGui::GetWindowDrawList()->PushClipRect(viewport_min, viewport_max);

		float* snap = nullptr;
		Vec4 scale_snap = Vec4(m_Editor->GetSettings().scaleSnap);
		Vec4 rotation_snap = Vec4(m_Editor->GetSettings().rotationSnap);
		Vec4 translation_snap = Vec4(m_Editor->GetSettings().translationSnap);

		switch (m_GizmoOperation)
		{
			case ImGuizmo::OPERATION::SCALE: snap = &scale_snap[0]; break;
			case ImGuizmo::OPERATION::ROTATE: snap = &rotation_snap[0]; break;
			case ImGuizmo::OPERATION::TRANSLATE: snap = &translation_snap[0]; break;
		}

		bool manipulated = ImGuizmo::Manipulate
		(
			glm::value_ptr(viewport.GetView()),
			glm::value_ptr(viewport.GetProjection()),
			m_GizmoOperation,
			ImGuizmo::MODE::LOCAL,
			glm::value_ptr(world_space_transform),
			nullptr,
			g_Input->IsKeyDown(Key::LCTRL) ? &snap[0] : nullptr
		);

		ImGui::GetWindowDrawList()->PopClipRect();

		m_WasUsingGizmo = m_IsUsingGizmo;
		m_IsUsingGizmo = ImGuizmo::IsUsing();

		world_space_transform = glm::translate(world_space_transform, -additional_translation);

		if (m_IsUsingGizmo && !m_WasUsingGizmo)
		{
			m_TransformUndo.entity = GetActiveEntity();
			m_TransformUndo.previous = transform;
			assert(m_TransformUndo.entity != Entity::Null);
		}

		if (manipulated)
		{
			transform.localTransform = world_to_local_transform * world_space_transform;
			transform.Decompose();
			m_Changed = true;
		}

		if (!m_IsUsingGizmo && m_WasUsingGizmo)
		{
			assert(m_TransformUndo.entity != Entity::Null);
			m_TransformUndo.current = transform;
			m_Editor->GetUndo()->PushUndo(m_TransformUndo);
		}
	}

	// the viewport image is a drag and drop target for dropping materials onto meshes
	if (ImGui::BeginDragDropTarget())
	{
		Entity picked = Entity::Null;

		if (GetRenderInterface().GetEntityPickResult(m_DropPickRequest, picked))
			m_DropTargetEntity = scene.Exists(picked) ? picked : Entity::Null;

		m_DropPickRequest = GetRenderInterface().RequestEntityPick(mouse_pixel.x, mouse_pixel.y);

		Mesh* mesh = scene.GetPtr<Mesh>(m_DropTargetEntity);
		Skeleton* skeleton = scene.GetPtr<Skeleton>(m_DropTargetEntity);

		if (m_DropTargetEntity != Entity::Null)
		{
			ImGui::BeginTooltip();

			if (mesh)
			{
				const Name* name = scene.GetPtr<Name>(m_DropTargetEntity);
				ImGui::Text("Apply to %s", name ? name->name.c_str() : "mesh");
			}
			else
				ImGui::TextColored(cStoppedColor, "Not a mesh");

			ImGui::EndTooltip();
		}

		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_entity"))
		{
			const Entity entity = *reinterpret_cast<const Entity*>( payload->Data );

			if (mesh && scene.Has<Material>(entity))
			{
				mesh->material = entity;
				m_Changed = true;
			}

			if (skeleton && scene.Has<Animation>(entity))
			{
				skeleton->animation = entity;
				m_Changed = true;
			}

			if (mesh)
				SetActiveEntity(m_DropTargetEntity);

			m_DropTargetEntity = Entity::Null;
			m_DropPickRequest = 0;
		}

		ImGui::EndDragDropTarget();
	}

	if (show_debug_icons)
	{
		for (const auto& [entity, light] : scene.Each<Light>())
			AddClickableQuad(viewport, entity, (ImTextureID)GetRenderInterface().GetLightTexture(), light.position, 0.1f);

		for (const auto& [entity, camera] : scene.Each<Camera>())
			AddClickableQuad(viewport, entity, (ImTextureID)GetRenderInterface().GetCameraTexture(), camera.GetPosition(), 0.1f);

		for (const auto& [entity, light, transform] : scene.Each<DirectionalLight, Transform>())
			AddClickableQuad(viewport, entity, (ImTextureID)GetRenderInterface().GetLightTexture(), transform.position, 0.1f);
	}

	bool can_select_entity = m_IsMouseOver;
	can_select_entity &= ImGui::IsMouseClicked(ImGuiMouseButton_Left);
	can_select_entity &= ( SDL_GetModState() & ( SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_SHIFT | SDL_KMOD_GUI ) ) == 0;
	can_select_entity &= !ImGui::IsAnyItemHovered();
	can_select_entity &= !g_Input->IsKeyDown(Key::LSHIFT);
	can_select_entity &= !ImGuizmo::IsOver(m_GizmoOperation) || GetActiveEntity() == Entity::Null;

	if (can_select_entity)
	{
		const IVec2 ray_pos = GUI::GetMousePosWindow(viewport, viewport_min);

		float hit_dist = FLT_MAX;
		Entity hit_entity = Entity::Null;

		Ray ray(viewport, Vec2(ray_pos.x, ray_pos.y));

		for (const auto& quad : m_EntityQuads)
		{
			for (int i = 0; i < 2; i++)
			{
				Vec2 barycentrics;
				const Optional<float> hit_result = ray.HitsTriangle(quad.mVertices[0], quad.mVertices[1 + i], quad.mVertices[2 + i], barycentrics);

				if (hit_result.has_value() && hit_result.value() < hit_dist)
				{
					hit_dist = hit_result.value();
					hit_entity = quad.mEntity;
				}
			}
		}

		if (hit_entity != Entity::Null)
		{
			m_SelectPickRequest = 0;
			SelectPickedEntity(hit_entity);
		}
		else
			m_SelectPickRequest = GetRenderInterface().RequestEntityPick(mouse_pixel.x, mouse_pixel.y);
	}

	if (m_SelectPickRequest != 0)
	{
		Entity picked = Entity::Null;

		if (GetRenderInterface().GetEntityPickResult(m_SelectPickRequest, picked))
		{
			m_SelectPickRequest = 0;
			SelectPickedEntity(picked);
		}
	}

	m_EntityQuads.clear();

	ImGui::SetCursorPos(pre_scene_cursor_pos + ImGui::GetStyle().WindowPadding);

	DrawToolbar();

	ImGui::End();

	if (m_Visible && ( show_debug_text || show_debug_fps ))
	{
		const float padding = ImGui::GetStyle().WindowPadding.x;

		ImGui::SetNextWindowPos(ImVec2(viewport_max.x - padding, viewport_min.y + padding), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
		ImGui::SetNextWindowBgAlpha(0.35f);

		const ImGuiWindowFlags metric_window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking |
													 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;

		ImGui::Begin("##ViewportStats", nullptr, metric_window_flags);

		const ImVec4 col_pink  = ImVec4(1.0f, 0.078f, 0.576f, 1.0f);

#ifndef NDEBUG
		static bool debug_layer_enabled = OS::sCheckCommandLineOption("-debug_layer");
		static bool gpu_validation_enabled = OS::sCheckCommandLineOption("-gpu_validation");

		if (debug_layer_enabled)
			ImGui::TextColored(col_pink, "DEBUG DEVICE ENABLED");

		if (gpu_validation_enabled)
			ImGui::TextColored(col_pink, "GPU VALIDATION ENABLED");
#endif

		if (show_debug_fps)
			ImGui::Text("%.2f ms (%.0f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

		if (show_debug_text)
		{
			uint64_t triangle_count = 0;
			for (const Mesh& mesh : GetScene().GetStorage<Mesh>())
				triangle_count += mesh.indices.size();
			triangle_count /= 3;

			std::array<uint32_t, MATERIAL_BLEND_MODE_COUNT> material_counts = {};

			for (const auto& [entity, material] : GetScene().Each<Material>())
				material_counts[material.blendMode]++;

			ImGui::Separator();
			ImGui::Text("Draw calls: %u", GetScene().Count<Mesh>());
			ImGui::Text("Triangles: %llu", triangle_count);
			ImGui::Text("Materials: %u opaque, %u masked, %u blended", material_counts[MATERIAL_BLEND_MODE_OPAQUE], material_counts[MATERIAL_BLEND_MODE_MASKED], material_counts[MATERIAL_BLEND_MODE_BLENDED]);
			ImGui::Text("Lights: %u", GetScene().Count<Light>());
			ImGui::Separator();
			ImGui::Text("GPU Buffers: %llu", GetRenderInterface().GetGPUStats().mLiveBuffers.load());
			ImGui::Text("GPU Textures: %llu", GetRenderInterface().GetGPUStats().mLiveTextures.load());
			ImGui::Text("Resolution: %u x %u", viewport.GetRenderSize().x, viewport.GetRenderSize().y);
		}

		ImGui::End();
	}

	m_TotalTime += inDeltaTime;
}


void ViewportWidget::DrawToolbar()
{
	Scene& scene = GetScene();

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.10f, 0.11f, 0.75f));

	auto DrawGizmoButton = [&](const char* inLabel, ImGuizmo::OPERATION inOperation, const char* inTooltip)
	{
		const bool is_selected = ( m_GizmoOperation == inOperation ) && m_IsGizmoEnabled;

		if (is_selected)
			ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

		if (ImGui::Button(inLabel))
		{
			if (m_GizmoOperation == inOperation)
				m_IsGizmoEnabled = !m_IsGizmoEnabled;
			else
			{
				m_GizmoOperation = inOperation;
				m_IsGizmoEnabled = true;
			}
		}

		if (is_selected)
			ImGui::PopStyleColor();

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", inTooltip);
	};

	DrawGizmoButton((const char*)ICON_FA_ARROWS_ALT, ImGuizmo::OPERATION::TRANSLATE, "Move (T)");
	ImGui::SameLine(0.0f, 2.0f);
	DrawGizmoButton((const char*)ICON_FA_SYNC_ALT, ImGuizmo::OPERATION::ROTATE, "Rotate (R)");
	ImGui::SameLine(0.0f, 2.0f);
	DrawGizmoButton((const char*)ICON_FA_EXPAND_ARROWS_ALT, ImGuizmo::OPERATION::SCALE, "Scale (S)");

	ImGui::SameLine();

	ImGui::Button((const char*)ICON_FA_COG);

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Viewport, renderer and camera settings");

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));

	if (ImGui::BeginPopupContextItem("##ViewportSettings", ImGuiPopupFlags_MouseButtonLeft))
	{
		static int& show_border = g_CVariables->GetValue<int>("r_show_border");
		static int& show_debug_fps = g_CVariables->GetValue<int>("r_show_debug_fps");
		static int& show_debug_text = g_CVariables->GetValue<int>("r_show_debug_text");
		static int& show_debug_icons = g_CVariables->GetValue<int>("r_show_debug_icons");

		ImGui::SeparatorText("Viewport");

		ImGui::Checkbox("Play Mode Border", (bool*)&show_border);
		ImGui::Checkbox("Frame Time", (bool*)&show_debug_fps);
		ImGui::Checkbox("Statistics", (bool*)&show_debug_text);
		ImGui::Checkbox("Light & Camera Icons", (bool*)&show_debug_icons);

		ImGui::SeparatorText("Snapping (hold Ctrl)");

		ImGui::DragFloat("Move", &m_Editor->GetSettings().translationSnap, 0.01f, 0.01f, 100.0f, "%.2f m");
		ImGui::DragFloat("Rotate", &m_Editor->GetSettings().rotationSnap, 0.5f, 0.5f, 180.0f, "%.1f deg");
		ImGui::DragFloat("Scale", &m_Editor->GetSettings().scaleSnap, 0.01f, 0.01f, 10.0f, "%.2f");

		ImGui::SeparatorText("Debug View");

		const int current_debug_texture = int(m_Editor->GetRenderInterface()->GetDebugTextureIndex());
		const uint32_t debug_texture_count = m_Editor->GetRenderInterface()->GetDebugTextureCount();

		if (ImGui::BeginCombo("##DebugView", m_Editor->GetRenderInterface()->GetDebugTextureName(current_debug_texture)))
		{
			for (uint32_t texture_idx = 0; texture_idx < debug_texture_count; texture_idx++)
			{
				if (ImGui::Selectable(m_Editor->GetRenderInterface()->GetDebugTextureName(texture_idx), current_debug_texture == int(texture_idx)))
					m_Editor->GetRenderInterface()->SetDebugTextureIndex(texture_idx);
			}

			ImGui::EndCombo();
		}

		static int& wireframe_mode = g_CVariables->GetValue<int>("r_wireframe");
		static constexpr std::array cWireframeModeNames = { "Wireframe Off", "Wireframe Overlay", "Wireframe X-Ray" };

		if (ImGui::BeginCombo("##WireframeMode", cWireframeModeNames[glm::clamp(wireframe_mode, 0, int(cWireframeModeNames.size()) - 1)]))
		{
			for (int mode = 0; mode < int(cWireframeModeNames.size()); mode++)
			{
				if (ImGui::Selectable(cWireframeModeNames[mode], wireframe_mode == mode))
					wireframe_mode = mode;
			}

			ImGui::EndCombo();
		}

		m_Editor->GetRenderInterface()->DrawDebugSettings(m_Editor);

		ImGui::SeparatorText("Physics");

		bool debug_physics = GetPhysics().GetDebugRendering();
		if (ImGui::Checkbox("Visualize Physics", &debug_physics))
			GetPhysics().SetDebugRendering(debug_physics);

		if (ImGui::Button("Generate Rigid Bodies"))
		{
			Timer timer;
			for (const auto& [sb_entity, sb_transform, sb_mesh] : scene.Each<Transform, Mesh>())
			{
				if (!scene.Has<RigidBody>(sb_entity) && !scene.Has<SoftBody>(sb_entity))
					scene.Add<RigidBody>(sb_entity);
			}

			GetPhysics().GenerateRigidBodiesEntireScene(GetScene());
			m_Editor->MarkSceneChanged();

			gLogInfo("Physics", "Rigid body generation took {} seconds", timer.GetElapsedFormatted());
		}

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Adds a static rigid body to every mesh in the scene.");

		ImGui::SeparatorText("Camera");

		const Entity camera_entity = m_Editor->GetCameraEntity();
		const char* camera_name = "Editor Camera";

		if (scene.Has<Name, Camera>(camera_entity))
			camera_name = scene.Get<Name>(camera_entity).name.c_str();

		if (ImGui::BeginCombo("##ActiveCamera", camera_name))
		{
			if (ImGui::Selectable("Editor Camera", camera_entity == Entity::Null))
				m_Editor->SetCameraEntity(Entity::Null);

			for (const auto& [entity, camera] : scene.Each<Camera>())
			{
				const Name* name = scene.GetPtr<Name>(entity);

				ImGui::PushID(uint32_t(entity));

				if (ImGui::Selectable(name ? name->name.c_str() : "Camera", camera_entity == entity))
					m_Editor->SetCameraEntity(entity);

				ImGui::PopID();
			}

			ImGui::EndCombo();
		}

		if (camera_entity == Entity::Null)
		{
			Camera& camera = m_Editor->GetCamera();

			Vec3 position = camera.GetPosition();
			if (ImGui::DragFloat3("Position", glm::value_ptr(position), 0.001f, -FLT_MAX, FLT_MAX))
				camera.SetPosition(position);

			Vec2 orientation = camera.GetAngle();
			if (ImGui::DragFloat2("Orientation", glm::value_ptr(orientation), 0.001f, -FLT_MAX, FLT_MAX))
				camera.SetAngle(orientation);

			float field_of_view = camera.GetFov();
			if (ImGui::DragFloat("Field of View", &field_of_view, 0.1f, 1.0f, 179.0f, "%.1f deg"))
				camera.SetFov(field_of_view);
		}

		ImGui::EndPopup();
	}

	ImGui::PopStyleVar();

	const EGameState game_state = m_Editor->GetGameState();
	const float button_width = ImGui::GetFrameHeight();
	const float play_controls_width = button_width * 2.0f + ImGui::GetStyle().ItemSpacing.x;

	ImGui::SameLine();
	ImGui::SetCursorPosX(glm::max(ImGui::GetCursorPosX(), ( ImGui::GetWindowWidth() - play_controls_width ) * 0.5f));

	ImGui::PushStyleColor(ImGuiCol_Text, game_state == GAME_RUNNING ? cPausedColor : cRunningColor);

	if (ImGui::Button(game_state == GAME_RUNNING ? (const char*)ICON_FA_PAUSE : (const char*)ICON_FA_PLAY, ImVec2(button_width, 0.0f)))
	{
        if (game_state == GAME_STOPPED)
            m_Editor->Start();
        else if (game_state == GAME_RUNNING)
            m_Editor->Pause();
        else if (game_state == GAME_PAUSED)
            m_Editor->Unpause();
	}

	ImGui::PopStyleColor();

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(game_state == GAME_RUNNING ? "Pause" : game_state == GAME_PAUSED ? "Resume (F5)" : "Play (F5)");

	ImGui::SameLine();

	ImGui::BeginDisabled(game_state == GAME_STOPPED);
	ImGui::PushStyleColor(ImGuiCol_Text, game_state != GAME_STOPPED ? cStoppedColor : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

	if (ImGui::Button((const char*)ICON_FA_STOP, ImVec2(button_width, 0.0f)))
        m_Editor->Stop();

	ImGui::PopStyleColor();
	ImGui::EndDisabled();

	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("Stop (Escape)");

	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
}


void ViewportWidget::SelectPickedEntity(Entity inEntity)
{
	Scene& scene = GetScene();

	if (inEntity == Entity::Null || !scene.Exists(inEntity) || inEntity == scene.GetRootEntity())
	{
		SetActiveEntity(Entity::Null);
		return;
	}

	SetActiveEntity(inEntity);
}


void ViewportWidget::OnEvent(Widgets* inWidgets, const SDL_Event& inEvent)
{
	if (inEvent.type != SDL_EVENT_KEY_DOWN || inEvent.key.repeat || g_Input->IsRelativeMouseMode())
		return;

	if (( SDL_GetModState() & ( SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_SHIFT ) ) != 0)
		return;

	switch (inEvent.key.key)
	{
		case SDLK_T:
		{
			m_GizmoOperation = ImGuizmo::OPERATION::TRANSLATE;
			m_IsGizmoEnabled = true;
		} break;

		case SDLK_R:
		{
			m_GizmoOperation = ImGuizmo::OPERATION::ROTATE;
			m_IsGizmoEnabled = true;
		} break;

		case SDLK_S:
		{
			m_GizmoOperation = ImGuizmo::OPERATION::SCALE;
			m_IsGizmoEnabled = true;
		} break;
	}
}


void ViewportWidget::AddClickableQuad(const Viewport& inViewport, Entity inEntity, ImTextureID inTexture, Vec3 inPos, float inSize)
{
	Mat4x4 model = Mat4x4(1.0f);
	model = glm::translate(model, inPos);

	Vec3 view_vector = Vec3(0.0f);
	view_vector.x = inViewport.GetPosition().x - inPos.x;
	view_vector.z = inViewport.GetPosition().z - inPos.z;

	const Vec3 look_at_vector = Vec3(0.0f, 0.0f, 1.0f);
	const Vec3 view_vector_normalized = glm::normalize(view_vector);

	const Vec3 up = glm::cross(look_at_vector, view_vector_normalized);
	const float angle = glm::dot(look_at_vector, view_vector_normalized);

	model = glm::rotate(model, acos(angle), up);
	model = glm::scale(model, Vec3(glm::length(view_vector) * inSize));

	Mat4x4 vp = inViewport.GetProjection() * inViewport.GetView();

	std::array vertices =
	{
		Vec4(-0.5f, -0.5f, 0.0f, 1.0f),
		Vec4(-0.5f,  0.5f, 0.0f, 1.0f),
		Vec4( 0.5f,  0.5f, 0.0f, 1.0f),
		Vec4( 0.5f, -0.5f, 0.0f, 1.0f)
	};

	for (Vec4& vertex : vertices)
		vertex = model * vertex;

	const ClickableQuad ws_quad = ClickableQuad { inEntity, vertices };

	const Frustum frustum = inViewport.GetFrustum();

	int visible_vertices = vertices.size();

	for (const Vec4& vertex : vertices)
	{
		if (!frustum.Contains(vertex))
			visible_vertices--;
	}

	if (visible_vertices == 0)
		return;

	for (const auto& [index, vertex] : gEnumerate(vertices))
	{
		vertex = vp * vertex;
		vertex /= vertex.w;

		vertex.x = m_WindowPos.x + ( vertex.x + 1.0f ) * 0.5f * m_WindowSize.x;
		vertex.y = m_WindowPos.y + ( 1.0f - vertex.y ) * 0.5f * m_WindowSize.y;
	}

	// Flip the 1.0 for DirectX, 0 for OpenGL and Vulkan
	float y_uv = 1.0f;

	ImGui::GetWindowDrawList()->AddImageQuad(
		inTexture,
		ImVec2(vertices[0].x, vertices[0].y),
		ImVec2(vertices[1].x, vertices[1].y),
		ImVec2(vertices[2].x, vertices[2].y),
		ImVec2(vertices[3].x, vertices[3].y),
		ImVec2(0, y_uv),
		ImVec2(0, 1 - y_uv),
		ImVec2(1, 1 - y_uv),
		ImVec2(1, y_uv)
	);

	m_EntityQuads.push_back(ws_quad);
}



}
