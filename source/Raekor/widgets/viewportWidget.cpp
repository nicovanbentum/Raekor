#include "pch.h"
#include "viewportWidget.h"
#include "menubarWidget.h"
#include "OS.h"
#include "gui.h"
#include "scene.h"
#include "timer.h"
#include "physics.h"
#include "components.h"
#include "primitives.h"
#include "application.h"

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

	static auto& show_debug_text = g_CVars.Create("r_show_debug_text", 1, IF_DEBUG_ELSE(true, false));

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
		const auto pixel = m_Editor->GetRenderInterface()->GetSelectedEntity(GetScene(), mouse_pos.x, mouse_pos.y);
		const auto picked = Entity(pixel);

		Mesh* mesh = nullptr;

		if (scene.Exists(picked))
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

	{
		for (const auto& [entity, light] : scene.Each<Light>())
		{
			AddClickableQuad(viewport, entity, (ImTextureID)GetRenderInterface().GetLightTexture(), light.position, 0.1f);
		}

		for (const auto& [entity, light, transform] : scene.Each<DirectionalLight, Transform>())
		{
			AddClickableQuad(viewport, entity, (ImTextureID)GetRenderInterface().GetLightTexture(), transform.position, 0.1f);
		}
	}

	auto pos = ImGui::GetWindowPos();
	viewport.offset = { pos.x, pos.y };

	if (ImGui::GetIO().MouseClicked[0] && mouseInViewport && !( GetActiveEntity() != NULL_ENTITY && ImGuizmo::IsOver(operation) ) && !ImGui::IsAnyItemHovered())
	{
		const auto mouse_pos = GUI::GetMousePosWindow(viewport, ImGui::GetWindowPos() + ( ImGui::GetWindowSize() - size ));
		const auto pixel = m_Editor->GetRenderInterface()->GetSelectedEntity(GetScene(), mouse_pos.x, mouse_pos.y);

		/*float hit_dist = FLT_MAX;
		Entity hit_entity = NULL_ENTITY;

		Ray ray(viewport, Vec2(mouse_pos.x, mouse_pos.y));

		for (const auto& quad : m_EntityQuads)
		{
			Vec2 barycentrics;
			const auto hit_result = ray.HitsTriangle(quad.mVertices[0], quad.mVertices[1], quad.mVertices[2], barycentrics);
		}*/

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

						Vec2 barycentrics;
						const auto hit_result = ray.HitsTriangle(v0, v1, v2, barycentrics);

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

	if (GetActiveEntity() != NULL_ENTITY && scene.Has<Transform>(GetActiveEntity()) && gizmoEnabled)
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

	auto menubar_widget = inWidgets->GetWidget<MenubarWidget>();

	if (menubar_widget && !menubar_widget->IsOpen())
	{
		ImGui::SameLine();

		if (ImGui::Button((const char*)ICON_FA_ADDRESS_BOOK))
			menubar_widget->Show();
	}

	const auto cursor_pos = ImGui::GetCursorPos();
	
	ImGui::SameLine();
	ImGui::Button((const char*)ICON_FA_COG);
	
	ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 5.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));


 if (ImGui::BeginPopupContextItem(NULL, ImGuiPopupFlags_MouseButtonLeft))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextAlign, ImVec2(0.5f, 0.5f));
		ImGui::SeparatorText("Viewport Settings");

		ImGui::Checkbox("Show Debug Text", (bool*)&show_debug_text);

		auto& current_debug_texture = m_Editor->GetRenderInterface()->GetSettings().mDebugTexture;
		const auto debug_texture_count = m_Editor->GetRenderInterface()->GetDebugTextureCount();

		ImGui::AlignTextToFramePadding();

		if (ImGui::BeginMenu("Debug Render Output"))
		{
			for (auto texture_idx = 0u; texture_idx < debug_texture_count; texture_idx++)
			{
				if (ImGui::RadioButton(m_Editor->GetRenderInterface()->GetDebugTextureName(texture_idx), current_debug_texture == texture_idx))
				{
					current_debug_texture = texture_idx;
					m_Editor->GetRenderInterface()->OnResize(viewport); // not an actual resize, just to recreate render targets
				}
			}

			ImGui::EndMenu();
		}

		// Draw all the renderer debug UI
		m_Editor->GetRenderInterface()->DrawDebugSettings(m_Editor, GetScene(), m_Editor->GetViewport());

		ImGui::SeparatorText("Physics Settings");

		auto debug_physics = GetPhysics().GetDebugRendering();
		if (ImGui::Checkbox("Debug Visualize Physics", &debug_physics))
			GetPhysics().SetDebugRendering(debug_physics);

		if (ImGui::Button("Generate Rigid Bodies"))
		{
			Timer timer;
			for (const auto& [sb_entity, sb_transform, sb_mesh] : scene.Each<Transform, Mesh>())
			{
				if (!scene.Has<SoftBody>(sb_entity))
					scene.Add<BoxCollider>(sb_entity);
			}

			GetPhysics().GenerateRigidBodiesEntireScene(GetScene());

			g_ThreadPool.WaitForJobs();
			m_Editor->LogMessage("Rigid Body Generation took " + timer.GetElapsedFormatted() + " seconds.");
		}

		ImGui::SameLine();

		if (ImGui::Button("Spawn/Reset Balls"))
		{
			const auto material_entity = scene.Create();
			auto& material_name = scene.Add<Name>(material_entity);
			material_name = "Ball Material";
			auto& ball_material = scene.Add<Material>(material_entity);
			ball_material.albedo = glm::vec4(1.0f, 0.25f, 0.38f, 1.0f);

			if (auto renderer = m_Editor->GetRenderInterface())
				renderer->UploadMaterialTextures(material_entity, ball_material, GetAssets());

			for (uint32_t i = 0; i < 64; i++)
			{
				auto entity = scene.CreateSpatialEntity("ball");
				auto& transform = scene.Get<Transform>(entity);
				auto& mesh = scene.Add<Mesh>(entity);

				mesh.material = material_entity;

				constexpr auto radius = 4.5f;
				gGenerateSphere(mesh, radius, 32, 32);
				GetRenderInterface().UploadMeshBuffers(entity, mesh);
				GetRenderInterface().UploadMaterialTextures(material_entity, ball_material, GetAssets());

				transform.position = Vec3(-65.0f, 85.0f + i * ( radius * 2.0f ), 0.0f);
				transform.Compose();

				auto& collider = scene.Add<BoxCollider>(entity);
				collider.motionType = JPH::EMotionType::Dynamic;
				JPH::ShapeSettings* settings = new JPH::SphereShapeSettings(radius);

				auto body_settings = JPH::BodyCreationSettings(
					settings,
					JPH::Vec3(transform.position.x, transform.position.y, transform.position.z),
					JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w),
					collider.motionType,
					EPhysicsObjectLayers::MOVING
				);

				body_settings.mFriction = 0.1;
				body_settings.mRestitution = 0.35;

				auto& body_interface = GetPhysics().GetSystem()->GetBodyInterface();
				collider.bodyID = body_interface.CreateAndAddBody(body_settings, JPH::EActivation::Activate);

				//body_interface.AddImpulse(collider.bodyID, JPH::Vec3Arg(50.0f, -2.0f, 50.0f));
			}
		}

		ImGui::SeparatorText("Camera Settings");

		auto position = viewport.GetCamera().GetPosition();
		if (ImGui::DragFloat3("Position", glm::value_ptr(position)))
			viewport.GetCamera().SetPosition(position);

		auto orientation = viewport.GetCamera().GetAngle();
		if (ImGui::DragFloat2("Orientation", glm::value_ptr(orientation), 0.001f, -FLT_MAX, FLT_MAX))
			viewport.GetCamera().SetAngle(orientation);

		auto field_of_view = viewport.GetFieldOfView();
		if (ImGui::DragFloat("Field of View", &field_of_view, 0.1f)) 
			viewport.SetFieldOfView(field_of_view);

		ImGui::PopStyleVar();
		ImGui::EndPopup();
	}

	ImGui::PopStyleVar();
	ImGui::PopStyleVar();

	ImGui::SetCursorPos(cursor_pos);

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

	ImGui::PopStyleColor();

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
			static auto debug_layer_enabled = OS::sCheckCommandLineOption("-debug_layer");
			static auto gpu_validation_enabled = OS::sCheckCommandLineOption("-gpu_validation");

			if (debug_layer_enabled)
				ImGui::TextColored(ImVec(col_pink), "DEBUG DEVICE ENABLED");

			if (gpu_validation_enabled)
				ImGui::TextColored(ImVec(col_pink), "GPU VALIDATION ENABLED");
		}
#endif
		uint64_t triangle_count = 0;
		for (const auto& mesh : GetScene().GetStorage<Mesh>())
			triangle_count += mesh.indices.size();
		triangle_count /= 3;

		if (show_debug_text)
		{
			ImGui::Text("Buffers: %i", GetRenderInterface().GetGPUStats().mLiveBuffers.load());
			ImGui::Text("Textures: %i", GetRenderInterface().GetGPUStats().mLiveTextures.load());
#if 0
			ImGui::Text("RTV Heap: %i", GetRenderInterface().GetGPUStats().mLiveRTVHeap.load());
			ImGui::Text("DSV Heap: %i", GetRenderInterface().GetGPUStats().mLiveDSVHeap.load());
			ImGui::Text("Sampler Heap: %i", GetRenderInterface().GetGPUStats().mLiveSamplerHeap.load());
			ImGui::Text("Resource Heap: %i", GetRenderInterface().GetGPUStats().mLiveResourceHeap.load());
#endif
			ImGui::Text("Materials: %i", GetScene().Count<Material>());
			ImGui::Text("Draw calls: %i", GetScene().Count<Mesh>());
			ImGui::Text("Transforms: %i", GetScene().Count<Transform>());
			ImGui::Text("Triangle Count: %i", triangle_count);
			ImGui::Text("Render Resolution: %i x %i", viewport.GetRenderSize().x, viewport.GetRenderSize().y);
			ImGui::Text("Display Resolution: %i x %i", viewport.GetDisplaySize().x, viewport.GetDisplaySize().y);
			ImGui::Text("Frame %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		}

		ImGui::End();
	}
	
	m_TotalTime += inDeltaTime;
}


void ViewportWidget::OnEvent(Widgets* inWidgets, const SDL_Event& ev)
{

	if (ev.type == SDL_KEYDOWN && !ev.key.repeat && !SDL_GetRelativeMouseMode())
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


void ViewportWidget::AddClickableQuad(const Viewport& inViewport, Entity inEntity, ImTextureID inTexture, Vec3 inPos, float inSize)
{
	auto model = Mat4x4(1.0f);
	model = glm::translate(model, inPos);

	auto view_vector = Vec3(0.0f);
	view_vector.x = inViewport.GetCamera().GetPosition().x - inPos.x;
	view_vector.z = inViewport.GetCamera().GetPosition().z - inPos.z;

	const auto look_at_vector = Vec3(0.0f, 0.0f, 1.0f);
	const auto view_vector_normalized = glm::normalize(view_vector);

	const auto up = glm::cross(look_at_vector, view_vector_normalized);
	const auto angle = glm::dot(look_at_vector, view_vector_normalized);

	model = glm::rotate(model, acos(angle), up);
	model = glm::scale(model, Vec3(glm::length(view_vector) * inSize));

	auto vp = inViewport.GetCamera().GetProjection() * inViewport.GetCamera().GetView();

	auto vertices = std::array
	{
		Vec4(-0.5f, -0.5f, 0.0f, 1.0f),
		Vec4(-0.5f,  0.5f, 0.0f, 1.0f),
		Vec4( 0.5f,  0.5f, 0.0f, 1.0f),
		Vec4( 0.5f, -0.5f, 0.0f, 1.0f)
	};

	for (auto& vertex : vertices)
		vertex = model * vertex;

	const auto frustum = inViewport.GetCamera().GetFrustum();

	int visible_vertices = vertices.size();

	for (const auto& vertex : vertices)
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

		vertex.x = ImGui::GetWindowPos().x + ( vertex.x + 1.0f ) * 0.5f * ImGui::GetWindowSize().x;
		vertex.y = ImGui::GetWindowPos().y + ( 1.0f - vertex.y ) * 0.5f * ImGui::GetWindowSize().y;
	}

	// Flip the Y uv for OpenGL and Vulkan
	float y_uv = m_Editor->GetRenderInterface()->GetGraphicsAPI() & GraphicsAPI::DirectX ? 1.0f : 0.0f;

	ImGui::GetWindowDrawList()->AddImageQuad(
		(void*)( (intptr_t)GetRenderInterface().GetLightTexture() ),
		ImVec2(vertices[0].x, vertices[0].y),
		ImVec2(vertices[1].x, vertices[1].y),
		ImVec2(vertices[2].x, vertices[2].y),
		ImVec2(vertices[3].x, vertices[3].y),
		ImVec2(0, y_uv),
		ImVec2(0, 1 - y_uv),
		ImVec2(1, 1 - y_uv),
		ImVec2(1, y_uv)
	);

	/*ClickableQuad quad = { inEntity };

	for (const auto& [index, vertex] : gEnumerate(vertices))
		quad.mVertices[index] = model * vertex;

	m_EntityQuads.push_back(quad);*/
}



}