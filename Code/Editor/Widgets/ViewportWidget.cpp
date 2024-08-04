#include "pch.h"
#include "viewportWidget.h"

#include "OS.h"
#include "gui.h"
#include "iter.h"
#include "scene.h"
#include "timer.h"
#include "input.h"
#include "script.h"
#include "physics.h"
#include "components.h"
#include "primitives.h"
#include "application.h"
#include "menubarWidget.h"

namespace RK {

RTTI_DEFINE_TYPE_NO_FACTORY(ViewportWidget) {}

ViewportWidget::ViewportWidget(Application* inApp) :
	IWidget(inApp, reinterpret_cast<const char*>( ICON_FA_VIDEO " Viewport " )) {}


void ViewportWidget::Draw(Widgets* inWidgets, float inDeltaTime)
{
	Scene& scene = IWidget::GetScene();
	Physics& physics = IWidget::GetPhysics();
	Viewport& viewport = m_Editor->GetViewport();

	static int& show_debug_text = g_CVariables->Create("r_show_debug_text", 1, IF_DEBUG_ELSE(true, false));
	static int& show_debug_icons = g_CVariables->Create("r_show_debug_icons", 1, IF_DEBUG_ELSE(true, false));

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));

	const ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize |
								   ImGuiWindowFlags_NoScrollWithMouse |
								   ImGuiWindowFlags_NoScrollbar;

	ImGui::SetNextWindowSize(ImVec2(160, 90), ImGuiCond_FirstUseEver);
	m_Visible = ImGui::Begin(m_Title.c_str(), &m_Open, flags);
	m_Editor->GetRenderInterface()->GetSettings().paused = !m_Visible;

	ImVec2 pre_scene_cursor_pos = ImGui::GetCursorPos();

	// figure out if we need to resize the viewport
	ImVec2 size = ImGui::GetContentRegionAvail();
	size.x = glm::max(size.x, 160.0f);
	size.y = glm::max(size.y, 90.0f);

	bool resized = false;
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
	ImVec2 uv0 = ImVec2(0, 0);
	ImVec2 uv1 = ImVec2(1, 1);

	// Render the display image
	static const std::array border_state_colors =
	{
		ImGui::GetStyleColorVec4(ImGuiCol_CheckMark),
		ImVec4(0.35f, 0.78f, 1.0f, 1.0f),
		ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
	};

	const ImVec2 image_size = ImVec2(size.x, size.y);
	const ImVec4 border_color = GetPhysics().GetState() != Physics::Idle ? border_state_colors[GetPhysics().GetState()] : ImVec4(0, 0, 0, 1);
	ImGui::Image((void*)( (intptr_t)m_DisplayTexture ), size, uv0, uv1, ImVec4(1, 1, 1, 1), border_color);

	mouseInViewport = ImGui::IsItemHovered();
	const ImVec2 viewportMin = ImGui::GetItemRectMin();
	const ImVec2 viewportMax = ImGui::GetItemRectMax();

	// the viewport image is a drag and drop target for dropping materials onto meshes
	if (ImGui::BeginDragDropTarget())
	{
		const IVec2 mouse_pos = GUI::GetMousePosWindow(viewport, ImGui::GetWindowPos() + ( ImGui::GetWindowSize() - size ));
		const uint32_t pixel = m_Editor->GetRenderInterface()->GetSelectedEntity(GetScene(), mouse_pos.x, mouse_pos.y);
		const Entity picked = Entity(pixel);

		Mesh* mesh = nullptr;
		Skeleton* skeleton = nullptr;

		if (scene.Exists(picked))
		{
			ImGui::BeginTooltip();

			mesh = scene.GetPtr<Mesh>(picked);
			skeleton = scene.GetPtr<Skeleton>(picked);

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

		const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_entity");

		if (payload && mesh)
		{
			Entity entity = *reinterpret_cast<const Entity*>( payload->Data );

			if (scene.Has<Material>(entity))
				mesh->material = entity;

			if (scene.Has<Animation>(entity) && skeleton)
				skeleton->animation = entity;
				

			SetActiveEntity(picked);
		}

		ImGui::EndDragDropTarget();
	}

	if (show_debug_icons) 
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

	ImVec2 pos = ImGui::GetWindowPos();
	viewport.offset = { pos.x, pos.y };

	bool can_select_entity = mouseInViewport;
	can_select_entity &= ImGui::IsMouseClicked(ImGuiMouseButton_Left);
	can_select_entity &= SDL_GetModState() == KMOD_NONE;
	can_select_entity &= !ImGui::IsAnyItemHovered();
	can_select_entity &= !g_Input->IsKeyDown(Key::LSHIFT);
	can_select_entity &= !ImGuizmo::IsOver(operation);

	if (can_select_entity)
	{
		const IVec2 mouse_pos = GUI::GetMousePosWindow(viewport, ImGui::GetWindowPos() + ( ImGui::GetWindowSize() - size ));

		const uint32_t pixel = m_Editor->GetRenderInterface()->GetSelectedEntity(GetScene(), mouse_pos.x, mouse_pos.y);

		float hit_dist = FLT_MAX;
		Entity hit_entity = Entity::Null;

		Ray ray(viewport, Vec2(mouse_pos.x, mouse_pos.y));

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

		Entity picked = hit_entity == Entity::Null ? Entity(pixel) : hit_entity;

		if (GetActiveEntity() == picked)
		{
			if (SoftBody* soft_body = scene.GetPtr<SoftBody>(GetActiveEntity()))
			{
				if (JPH::Body* body = GetPhysics().GetSystem()->GetBodyLockInterface().TryGetBody(soft_body->mBodyID))
				{
					const Ray ray = Ray(viewport, mouse_pos);
					const Mesh& mesh = scene.Get<Mesh>(GetActiveEntity());
					const Transform& transform = scene.Get<Transform>(GetActiveEntity());

					float hit_dist = FLT_MAX;
					int triangle_index = -1;

					for (int i = 0; i < mesh.indices.size(); i += 3)
					{
						const Vec3 v0 = Vec3(transform.worldTransform * Vec4(mesh.positions[mesh.indices[i]], 1.0));
						const Vec3 v1 = Vec3(transform.worldTransform * Vec4(mesh.positions[mesh.indices[i + 1]], 1.0));
						const Vec3 v2 = Vec3(transform.worldTransform * Vec4(mesh.positions[mesh.indices[i + 2]], 1.0));

						Vec2 barycentrics;
						const Optional<float> hit_result = ray.HitsTriangle(v0, v1, v2, barycentrics);

						if (hit_result.has_value() && hit_result.value() < hit_dist)
						{
							hit_dist = hit_result.value();
							triangle_index = i;
						}
					}

					if (triangle_index > -1)
					{
						JPH::SoftBodyMotionProperties* props = (JPH::SoftBodyMotionProperties*)body->GetMotionProperties();
						props->GetVertex(mesh.indices[triangle_index]).mInvMass = 0.0f;
						props->GetVertex(mesh.indices[triangle_index + 1]).mInvMass = 0.0f;
						props->GetVertex(mesh.indices[triangle_index + 2]).mInvMass = 0.0f;

						m_Editor->LogMessage(std::format("Soft Body triangle hit: {}", triangle_index));
					}
				}
			}
			else
				SetActiveEntity(Entity::Null);
		}
		else
			SetActiveEntity(picked);
	}

	m_EntityQuads.clear();

	if (GetActiveEntity() != Entity::Null && scene.Has<Transform>(GetActiveEntity()) && gizmoEnabled)
	{
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(viewportMin.x, viewportMin.y, viewportMax.x - viewportMin.x, viewportMax.y - viewportMin.y);

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

		// prevent the gizmo from going outside of the viewport
		ImGui::GetWindowDrawList()->PushClipRect(viewportMin, viewportMax);

		const bool manipulated = ImGuizmo::Manipulate
		(
			glm::value_ptr(viewport.GetCamera().GetView()),
			glm::value_ptr(viewport.GetCamera().GetProjection()),
			operation, 
			ImGuizmo::MODE::WORLD,
			glm::value_ptr(world_space_transform)
		);

		world_space_transform = glm::translate(world_space_transform, -additional_translation);

		m_Changed = false;

		if (manipulated)
		{
			transform.localTransform = world_to_local_transform *  world_space_transform;
			transform.Decompose();
			m_Changed = true;
		}
	}

	ImVec2 metricsPosition = ImGui::GetWindowPos();

	if (ImGuiDockNode* dock_node = ImGui::GetCurrentWindow()->DockNode)
		if (!dock_node->IsHiddenTabBar())
			metricsPosition.y += 25.0f;

	ImGui::SetCursorPos(pre_scene_cursor_pos + ImGui::GetStyle().FramePadding * 2.0f);

	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_FrameBg));

	auto DrawGizmoButton = [&](const char* inLabel, ImGuizmo::OPERATION inOperation)
	{
		const bool is_selected = ( operation == inOperation ) && gizmoEnabled;

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

	MenubarWidget* menubar_widget = inWidgets->GetWidget<MenubarWidget>();

	if (menubar_widget && !menubar_widget->IsOpen())
	{
		ImGui::SameLine();

		if (ImGui::Button((const char*)ICON_FA_ADDRESS_BOOK))
			menubar_widget->Show();
	}

	const ImVec2 cursor_pos = ImGui::GetCursorPos();
	
	ImGui::SameLine();
	ImGui::Button((const char*)ICON_FA_COG);
	
	ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 5.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));


	if (ImGui::BeginPopupContextItem(NULL, ImGuiPopupFlags_MouseButtonLeft))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextAlign, ImVec2(0.5f, 0.5f));
		ImGui::SeparatorText("Viewport Settings");

		ImGui::Checkbox("Show Debug Text", (bool*)&show_debug_text);

		ImGui::Checkbox("Show Debug Icons", (bool*)&show_debug_icons);

		int& current_debug_texture = m_Editor->GetRenderInterface()->GetSettings().mDebugTexture;
		const uint32_t debug_texture_count = m_Editor->GetRenderInterface()->GetDebugTextureCount();

		ImGui::AlignTextToFramePadding();

		if (ImGui::BeginMenu("Debug Render Output"))
		{
			for (int texture_idx = 0; texture_idx < debug_texture_count; texture_idx++)
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

		bool debug_physics = GetPhysics().GetDebugRendering();
		if (ImGui::Checkbox("Debug Visualize Physics", &debug_physics))
			GetPhysics().SetDebugRendering(debug_physics);

		if (ImGui::Button("Generate Rigid Bodies"))
		{
			Timer timer;
			for (const auto& [sb_entity, sb_transform, sb_mesh] : scene.Each<Transform, Mesh>())
			{
				if (!scene.Has<SoftBody>(sb_entity))
					scene.Add<RigidBody>(sb_entity);
			}

			GetPhysics().GenerateRigidBodiesEntireScene(GetScene());

			g_ThreadPool.WaitForJobs();
			m_Editor->LogMessage("Rigid Body Generation took " + timer.GetElapsedFormatted() + " seconds.");
		}

		ImGui::SameLine();

		if (ImGui::Button("Spawn/Reset Balls"))
		{
			const Entity material_entity = scene.Create();
			Name& material_name = scene.Add<Name>(material_entity);
			material_name = "Ball Material";
			Material& ball_material = scene.Add<Material>(material_entity);
			ball_material.albedo = glm::vec4(1.0f, 0.25f, 0.38f, 1.0f);

			if (IRenderInterface* render_interface = m_Editor->GetRenderInterface())
				render_interface->UploadMaterialTextures(material_entity, ball_material, GetAssets());

			for (uint32_t i = 0; i < 64; i++)
			{
				Entity entity = scene.CreateSpatialEntity("ball");
				
				Mesh& mesh = scene.Add<Mesh>(entity);
				Transform& transform = scene.Get<Transform>(entity);

				mesh.material = material_entity;

				constexpr float radius = 4.5f;
				Mesh::CreateSphere(mesh, radius, 32, 32);
				GetRenderInterface().UploadMeshBuffers(entity, mesh);
				GetRenderInterface().UploadMaterialTextures(material_entity, ball_material, GetAssets());

				transform.position = Vec3(-65.0f, 85.0f + i * ( radius * 2.0f ), 0.0f);
				transform.Compose();

				RigidBody& collider = scene.Add<RigidBody>(entity);
				collider.motionType = JPH::EMotionType::Dynamic;
				JPH::ShapeSettings* settings = new JPH::SphereShapeSettings(radius);

				JPH::BodyCreationSettings body_settings = JPH::BodyCreationSettings
				(
					settings,
					JPH::Vec3(transform.position.x, transform.position.y, transform.position.z),
					JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w),
					collider.motionType,
					EPhysicsObjectLayers::MOVING
				);

				body_settings.mFriction = 0.1;
				body_settings.mRestitution = 0.35;

				JPH::BodyInterface& body_interface = GetPhysics().GetSystem()->GetBodyInterface();
				collider.bodyID = body_interface.CreateAndAddBody(body_settings, JPH::EActivation::Activate);

				//body_interface.AddImpulse(collider.bodyID, JPH::Vec3Arg(50.0f, -2.0f, 50.0f));
			}
		}

		ImGui::SeparatorText("Camera Settings");

		Vec3 position = viewport.GetCamera().GetPosition();
		if (ImGui::DragFloat3("Position", glm::value_ptr(position), 0.001f, -FLT_MAX, FLT_MAX))
			viewport.GetCamera().SetPosition(position);

		Vec2 orientation = viewport.GetCamera().GetAngle();
		if (ImGui::DragFloat2("Orientation", glm::value_ptr(orientation), 0.001f, -FLT_MAX, FLT_MAX))
			viewport.GetCamera().SetAngle(orientation);

		float field_of_view = viewport.GetFieldOfView();
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

	const Physics::EState physics_state = GetPhysics().GetState();
	ImGui::PushStyleColor(ImGuiCol_Text, border_state_colors[GetPhysics().GetState()]);

	if (ImGui::Button(physics_state == Physics::Stepping ? (const char*)ICON_FA_PAUSE : (const char*)ICON_FA_PLAY))
	{
		switch (physics_state)
		{
			case Physics::Idle:
			{
				GetPhysics().SaveState();
				GetPhysics().SetState(Physics::Stepping);

				g_Input->SetRelativeMouseMode(true);
				m_Editor->SetGameState(GAME_RUNNING);

				for (auto [entity, script] : scene.Each<NativeScript>())
				{
					if (script.script) 
					{
						try 
						{
							script.script->OnStart();
						}
						catch (std::exception& e) 
						{
							std::cout << e.what() << '\n';
						}
					}
				}

			} break;
			case Physics::Paused:
			{
				GetPhysics().SetState(Physics::Stepping);
				m_Editor->SetGameState(GAME_RUNNING);
			} break;
			case Physics::Stepping:
			{
				GetPhysics().SetState(Physics::Paused);
				m_Editor->SetGameState(GAME_PAUSED);
			} break;
		}
	}

	ImGui::PopStyleColor();
	ImGui::SameLine();

	const Physics::EState current_physics_state = physics_state;
	if (current_physics_state != Physics::Idle)
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));

	if (ImGui::Button((const char*)ICON_FA_STOP))
	{
		if (physics_state != Physics::Idle)
		{
			GetPhysics().RestoreState();
			GetPhysics().Step(scene, inDeltaTime); // Step once to trigger the restored state
			GetPhysics().SetState(Physics::Idle);

			m_Editor->SetGameState(GAME_STOPPED);
			g_Input->SetRelativeMouseMode(false);

			for (auto [entity, script] : scene.Each<NativeScript>())
			{
				if (script.script)
				{
					try
					{
						script.script->OnStop();
					}
					catch (std::exception& e)
					{
						std::cout << e.what() << '\n';
					}
				}
			}
		}
	}

	if (current_physics_state != Physics::Idle)
		ImGui::PopStyleColor();

	ImGui::PopStyleColor();

	ImGui::End();
	ImGui::PopStyleVar();

	if (m_Visible)
	{
		//Gui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
		ImGui::SetNextWindowPos(metricsPosition + ImVec2(size.x - 260.0f, 0.0f));
		ImGui::SetNextWindowBgAlpha(0.0f);

		const ImGuiWindowFlags metric_window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize;
		ImGui::Begin("GPU Metrics Shadow", (bool*)0, metric_window_flags);
		// ImGui::Text("Culled meshes: %i", renderer.m_GBuffer->culled);
		const GPUInfo& gpu_info = m_Editor->GetRenderInterface()->GetGPUInfo();

		const ImVec4 col_white = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		const ImVec4 col_pink  = ImVec4(1.0f, 0.078f, 0.576f, 1.0f);

#ifndef NDEBUG
		static bool debug_layer_enabled = OS::sCheckCommandLineOption("-debug_layer");
		static bool gpu_validation_enabled = OS::sCheckCommandLineOption("-gpu_validation");

		if (debug_layer_enabled)
			ImGui::TextColored(col_pink, "DEBUG DEVICE ENABLED");

		if (gpu_validation_enabled)
			ImGui::TextColored(col_pink, "GPU VALIDATION ENABLED");
#endif

		uint64_t triangle_count = 0;
		for (const Mesh& mesh : GetScene().GetStorage<Mesh>())
			triangle_count += mesh.indices.size();
		triangle_count /= 3;

		if (show_debug_text)
		{
            ImGui::Text("Frame: %i", GetRenderInterface().GetGPUStats().mFrameCounter);
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

		//Gui::PopStyleColor();
	}
	
	m_TotalTime += inDeltaTime;
}


void ViewportWidget::OnEvent(Widgets* inWidgets, const SDL_Event& inEvent)
{

	if (inEvent.type == SDL_KEYDOWN && !inEvent.key.repeat && !g_Input->IsRelativeMouseMode())
	{
		switch (inEvent.key.keysym.sym)
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
			case SDLK_DELETE:
			{
				GetScene().Destroy(GetActiveEntity());
				SetActiveEntity(Entity::Null);
			} break;
		}
	}
}


void ViewportWidget::AddClickableQuad(const Viewport& inViewport, Entity inEntity, ImTextureID inTexture, Vec3 inPos, float inSize)
{
	Mat4x4 model = Mat4x4(1.0f);
	model = glm::translate(model, inPos);

	Vec3 view_vector = Vec3(0.0f);
	view_vector.x = inViewport.GetCamera().GetPosition().x - inPos.x;
	view_vector.z = inViewport.GetCamera().GetPosition().z - inPos.z;

	const Vec3 look_at_vector = Vec3(0.0f, 0.0f, 1.0f);
	const Vec3 view_vector_normalized = glm::normalize(view_vector);

	const Vec3 up = glm::cross(look_at_vector, view_vector_normalized);
	const float angle = glm::dot(look_at_vector, view_vector_normalized);

	model = glm::rotate(model, acos(angle), up);
	model = glm::scale(model, Vec3(glm::length(view_vector) * inSize));

	Mat4x4 vp = inViewport.GetCamera().GetProjection() * inViewport.GetCamera().GetView();

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

	const Frustum frustum = inViewport.GetCamera().GetFrustum();

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

		vertex.x = ImGui::GetWindowPos().x + ( vertex.x + 1.0f ) * 0.5f * ImGui::GetWindowSize().x;
		vertex.y = ImGui::GetWindowPos().y + ( 1.0f - vertex.y ) * 0.5f * ImGui::GetWindowSize().y;
	}

	// Flip the 1.0 for DirectX, 0 for OpenGL and Vulkan
	float y_uv = 1.0f;

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

	m_EntityQuads.push_back(ws_quad);
}



}