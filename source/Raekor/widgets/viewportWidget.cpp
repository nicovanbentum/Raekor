#include "pch.h"
#include "viewportWidget.h"
#include "scene.h"
#include "gui.h"
#include "physics.h"
#include "application.h"
#include "components.h"

namespace Raekor {

RTTI_DEFINE_TYPE_NO_FACTORY(ViewportWidget) {}

ViewportWidget::ViewportWidget(Application* inApp) :
	IWidget(inApp, reinterpret_cast<const char*>( ICON_FA_VIDEO " Viewport " ))
{
}


void ViewportWidget::Draw(Widgets* inWidgets, float inDeltaTime)
{
	auto& scene = IWidget::GetScene();
	auto& physics = IWidget::GetPhysics();
	auto& viewport = m_Editor->GetViewport();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
	const auto flags = ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoScrollbar;

	ImGui::SetNextWindowSize(ImVec2(160, 90), ImGuiCond_FirstUseEver);
	m_Visible = ImGui::Begin(m_Title.c_str(), &m_Open, flags);
	m_Editor->GetRenderInterface()->GetSettings().paused = !m_Visible;

	auto pre_scene_cursor_pos = ImGui::GetCursorPos();

	// figure out if we need to resize the viewport
	auto size = ImGui::GetContentRegionAvail();
	size.x = glm::max(size.x, 160.0f);
	size.y = glm::max(size.y, 90.0f);

	auto resized = false;
	if (viewport.GetDisplaySize().x != size.x || viewport.GetDisplaySize().y != size.y)
	{
		viewport.SetRenderSize({ size.x, size.y });
		viewport.SetDisplaySize({ size.x, size.y });
		m_Editor->GetRenderInterface()->OnResize(viewport);
		resized = true;
	}

	// update focused state
	m_Focused = ImGui::IsWindowFocused();

	// Update the display texture incase it changed
	m_DisplayTexture = m_Editor->GetRenderInterface()->GetDisplayTexture();

	// calculate display UVs
	auto uv0 = ImVec2(0, 0);
	auto uv1 = ImVec2(1, 1);

	// Flip the Y uv for OpenGL and Vulkan
	if (( m_Editor->GetRenderInterface()->GetGraphicsAPI() & GraphicsAPI::DirectX ) == 0)
		std::swap(uv0.y, uv1.y);

	// Render the display image
	const auto image_size = ImVec2(size.x, size.y);
	const auto border_color = GetPhysics().GetState() != Physics::Idle ? GetPhysics().GetStateColor() : ImVec4(0, 0, 0, 1);
	ImGui::Image((void*)( (intptr_t)m_DisplayTexture ), size, uv0, uv1, ImVec4(1, 1, 1, 1), border_color);

	mouseInViewport = ImGui::IsItemHovered();
	const ImVec2 viewportMin = ImGui::GetItemRectMin();
	const ImVec2 viewportMax = ImGui::GetItemRectMax();

	// the viewport image is a drag and drop target for dropping materials onto meshes
	if (ImGui::BeginDragDropTarget())
	{
		const auto mouse_pos = GUI::GetMousePosWindow(viewport, ImGui::GetWindowPos() + ( ImGui::GetWindowSize() - size ));
		const auto pixel = m_Editor->GetRenderInterface()->GetSelectedEntity(mouse_pos.x, mouse_pos.y);
		const auto picked = Entity(pixel);

		Mesh* mesh = nullptr;

		if (scene.IsValid(picked))
		{
			ImGui::BeginTooltip();

			mesh = scene.GetPtr<Mesh>(picked);

			if (scene.Has<Mesh>(picked))
			{
				ImGui::Text(std::string(std::string("Apply to ") + scene.Get<Name>(picked).name).c_str());
				SetActiveEntity(picked);
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
				ImGui::Text("Invalid target");
				ImGui::PopStyleColor();
			}

			ImGui::EndTooltip();
		}

		const auto payload = ImGui::AcceptDragDropPayload("drag_drop_mesh_material");

		if (payload && mesh)
		{
			mesh->material = *reinterpret_cast<const Entity*>( payload->Data );
			SetActiveEntity(mesh->material);
		}

		ImGui::EndDragDropTarget();
	}

	auto pos = ImGui::GetWindowPos();
	viewport.offset = { pos.x, pos.y };

	if (ImGui::GetIO().MouseClicked[0] && mouseInViewport && !( GetActiveEntity() != NULL_ENTITY && ImGuizmo::IsOver(operation) ) && !ImGui::IsAnyItemHovered())
	{
		const auto mouse_pos = GUI::GetMousePosWindow(viewport, ImGui::GetWindowPos() + ( ImGui::GetWindowSize() - size ));
		const auto pixel = m_Editor->GetRenderInterface()->GetSelectedEntity(mouse_pos.x, mouse_pos.y);

		auto picked = Entity(pixel);
		if (GetActiveEntity() == picked)
		{
			if (auto soft_body = scene.GetPtr<SoftBody>(GetActiveEntity()))
			{
				if (auto body = GetPhysics().GetSystem()->GetBodyLockInterface().TryGetBody(soft_body->mBodyID))
				{
					const auto ray = Ray(viewport, mouse_pos);
					const auto& mesh = scene.Get<Mesh>(GetActiveEntity());
					const auto& transform = scene.Get<Transform>(GetActiveEntity());

					auto hit_dist = FLT_MAX;
					auto triangle_index = -1;

					for (auto i = 0u; i < mesh.indices.size(); i += 3)
					{
						const auto v0 = Vec3(transform.worldTransform * Vec4(mesh.positions[mesh.indices[i]], 1.0));
						const auto v1 = Vec3(transform.worldTransform * Vec4(mesh.positions[mesh.indices[i + 1]], 1.0));
						const auto v2 = Vec3(transform.worldTransform * Vec4(mesh.positions[mesh.indices[i + 2]], 1.0));

						const auto hit_result = ray.HitsTriangle(v0, v1, v2);

						if (hit_result.has_value() && hit_result.value() < hit_dist)
						{
							hit_dist = hit_result.value();
							triangle_index = i;
						}
					}

					if (triangle_index > -1)
					{
						auto props = (JPH::SoftBodyMotionProperties*)body->GetMotionProperties();
						props->GetVertex(mesh.indices[triangle_index]).mInvMass = 0.0f;
						props->GetVertex(mesh.indices[triangle_index + 1]).mInvMass = 0.0f;
						props->GetVertex(mesh.indices[triangle_index + 2]).mInvMass = 0.0f;

						m_Editor->LogMessage(std::format("Soft Body triangle hit: {}", triangle_index));
					}
				}
			}
			else
				SetActiveEntity(NULL_ENTITY);
		}
		else
			SetActiveEntity(picked);
	}

	if (GetActiveEntity() != NULL_ENTITY && scene.IsValid(GetActiveEntity()) && scene.Has<Transform>(GetActiveEntity()) && gizmoEnabled)
	{
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(viewportMin.x, viewportMin.y, viewportMax.x - viewportMin.x, viewportMax.y - viewportMin.y);

		// temporarily transform to mesh space for gizmo use
		auto& transform = scene.Get<Transform>(GetActiveEntity());
		const auto mesh = scene.GetPtr<Mesh>(GetActiveEntity());

		if (mesh)
		{
			const auto bounds = BBox3D(mesh->aabb[0], mesh->aabb[1]).Scale(transform.GetScaleWorldSpace());
			transform.localTransform = glm::translate(transform.localTransform, bounds.GetCenter());
		}

		// prevent the gizmo from going outside of the viewport
		ImGui::GetWindowDrawList()->PushClipRect(viewportMin, viewportMax);

		const auto manipulated = ImGuizmo::Manipulate(
			glm::value_ptr(viewport.GetCamera().GetView()),
			glm::value_ptr(viewport.GetCamera().GetProjection()),
			operation, ImGuizmo::MODE::WORLD,
			glm::value_ptr(transform.localTransform)
		);

		// transform back to world space
		if (mesh)
		{
			const auto bounds = BBox3D(mesh->aabb[0], mesh->aabb[1]).Scale(transform.GetScaleWorldSpace());
			transform.localTransform = glm::translate(transform.localTransform, -bounds.GetCenter());
		}

		m_Changed = false;

		if (manipulated)
		{
			transform.Decompose();
			m_Changed = true;
		}
	}

	auto metricsPosition = ImGui::GetWindowPos();
	metricsPosition.y += ImGui::GetFrameHeightWithSpacing();

	if (auto dock_node = ImGui::GetCurrentWindow()->DockNode)
		if (!dock_node->IsHiddenTabBar())
			metricsPosition.y += 25.0f;

	ImGui::SetCursorPos(pre_scene_cursor_pos + ImGui::GetStyle().FramePadding * 2.0f);

	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_FrameBg));

	auto DrawGizmoButton = [&](const char* inLabel, ImGuizmo::OPERATION inOperation)
	{
		const auto is_selected = ( operation == inOperation ) && gizmoEnabled;

		if (is_selected)
			ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonHovered));

		if (ImGui::Button(inLabel))
		{
			if (operation == inOperation)
				gizmoEnabled = !gizmoEnabled;
			else
			{
				operation = inOperation;
				gizmoEnabled = true;
			}
		}

		if (is_selected)
			ImGui::PopStyleColor();
	};

	/*ImGui::Checkbox("Gizmo", &gizmoEnabled);
	ImGui::SameLine();*/

	DrawGizmoButton((const char*)ICON_FA_ARROWS_ALT, ImGuizmo::OPERATION::TRANSLATE);
	ImGui::SameLine();
	DrawGizmoButton((const char*)ICON_FA_SYNC_ALT, ImGuizmo::OPERATION::ROTATE);
	ImGui::SameLine();
	DrawGizmoButton((const char*)ICON_FA_EXPAND_ARROWS_ALT, ImGuizmo::OPERATION::SCALE);

	ImGui::SameLine();
	ImGui::SetCursorPosX(( ImGui::GetContentRegionAvail().x / 2 ));

	if (ImGui::Button((const char*)ICON_FA_HAMMER)) {}

	ImGui::SameLine();

	const auto physics_state = GetPhysics().GetState();
	ImGui::PushStyleColor(ImGuiCol_Text, GetPhysics().GetStateColor());
	if (ImGui::Button(physics_state == Physics::Stepping ? (const char*)ICON_FA_PAUSE : (const char*)ICON_FA_PLAY))
	{
		switch (physics_state)
		{
			case Physics::Idle:
			{
				GetPhysics().SaveState();
				GetPhysics().SetState(Physics::Stepping);
			} break;
			case Physics::Paused:
			{
				GetPhysics().SetState(Physics::Stepping);
			} break;
			case Physics::Stepping:
			{
				GetPhysics().SetState(Physics::Paused);
			} break;
		}
	}

	ImGui::PopStyleColor();
	ImGui::SameLine();

	const auto current_physics_state = physics_state;
	if (current_physics_state != Physics::Idle)
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));

	if (ImGui::Button((const char*)ICON_FA_STOP))
	{
		if (physics_state != Physics::Idle)
		{
			GetPhysics().RestoreState();
			GetPhysics().Step(scene, inDeltaTime); // Step once to trigger the restored state
			GetPhysics().SetState(Physics::Idle);
		}
	}

	if (current_physics_state != Physics::Idle)
		ImGui::PopStyleColor();

	ImGui::SameLine(ImGui::GetContentRegionAvail().x - 256.0f);

	auto& current_debug_texture = m_Editor->GetRenderInterface()->GetSettings().mDebugTexture;
	const auto debug_texture_count = m_Editor->GetRenderInterface()->GetDebugTextureCount();
	const auto preview = std::string("Render Output: " + std::string(m_Editor->GetRenderInterface()->GetDebugTextureName(current_debug_texture))); // allocs, BLEH TODO: FIXME
	
	ImGui::PopStyleColor();

	if (ImGui::BeginCombo("##RenderTarget", preview.c_str()))
	{
		for (auto texture_idx = 0u; texture_idx < debug_texture_count; texture_idx++)
		{
			if (ImGui::Selectable(m_Editor->GetRenderInterface()->GetDebugTextureName(texture_idx), current_debug_texture == texture_idx))
			{
				current_debug_texture = texture_idx;
				m_Editor->GetRenderInterface()->OnResize(viewport); // not an actual resize, just to recreate render targets
			}
		}

		ImGui::EndCombo();
	}

	ImGui::End();
	ImGui::PopStyleVar();

	if (m_Visible)
	{
		ImGui::SetNextWindowPos(metricsPosition + ImVec2(size.x - 260.0f, 0.0f));
		ImGui::SetNextWindowBgAlpha(0.0f);

		const auto metricWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize;
		ImGui::Begin("GPU Metrics", (bool*)0, metricWindowFlags);
		// ImGui::Text("Culled meshes: %i", renderer.m_GBuffer->culled);
		const auto& gpu_info = m_Editor->GetRenderInterface()->GetGPUInfo();

		const auto col_white = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
		const auto col_pink  = Vec4(1.0f, 0.078f, 0.576f, 1.0f);

#ifndef NDEBUG
		if (GetRenderInterface().GetGraphicsAPI() == GraphicsAPI::DirectX12 || GetRenderInterface().GetGraphicsAPI() == GraphicsAPI::Vulkan)
		{
			if (auto cvar = g_CVars.TryGetValue<int>("debug_layer"))
				if (*cvar)
					ImGui::TextColored(ImVec(col_pink), "DEBUG DEVICE ENABLED");

			if (auto cvar = g_CVars.TryGetValue<int>("gpu_validation"))
				if (*cvar)
					ImGui::TextColored(ImVec(col_pink), "GPU VALIDATION ENABLED");
		}
#endif
		uint64_t triangle_count = 0;
		for (const auto& mesh : GetScene().GetStorage<Mesh>())
			triangle_count += mesh.indices.size();
		triangle_count /= 3;

		ImGui::Text("Buffers: %i", GetRenderInterface().GetGPUStats().mLiveBuffers.load());
		ImGui::Text("Textures: %i", GetRenderInterface().GetGPUStats().mLiveTextures.load());
		ImGui::Text("RTV Heap: %i", GetRenderInterface().GetGPUStats().mLiveRTVHeap.load());
		ImGui::Text("DSV Heap: %i", GetRenderInterface().GetGPUStats().mLiveDSVHeap.load());
		ImGui::Text("Sampler Heap: %i", GetRenderInterface().GetGPUStats().mLiveSamplerHeap.load());
		ImGui::Text("Resource Heap: %i", GetRenderInterface().GetGPUStats().mLiveResourceHeap.load());
		ImGui::Text("Materials: %i", GetScene().Count<Material>());
		ImGui::Text("Draw calls: %i", GetScene().Count<Mesh>());
		ImGui::Text("Transforms: %i", GetScene().Count<Transform>());
		ImGui::Text("Triangle Count: %i", triangle_count);
		ImGui::Text("Render Resolution: %i x %i", viewport.GetRenderSize().x, viewport.GetRenderSize().y);
		ImGui::Text("Display Resolution: %i x %i", viewport.GetDisplaySize().x, viewport.GetDisplaySize().y);
		ImGui::Text("Frame %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

		ImGui::End();
	}
	
	m_TotalTime += inDeltaTime;
}


void ViewportWidget::OnEvent(Widgets* inWidgets, const SDL_Event& ev)
{
	if (ev.type == SDL_KEYDOWN && !ev.key.repeat)
	{
		switch (ev.key.keysym.sym)
		{
			case SDLK_r:
			{
				operation = ImGuizmo::OPERATION::ROTATE;
				break;
			}
			case SDLK_t:
			{
				operation = ImGuizmo::OPERATION::TRANSLATE;
				break;
			}
			case SDLK_s:
			{
				operation = ImGuizmo::OPERATION::SCALE;
				break;
			}
		}
	}
}



}