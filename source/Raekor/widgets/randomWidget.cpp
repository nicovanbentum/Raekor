#include "pch.h"
#include "randomWidget.h"
#include "timer.h"
#include "rmath.h"
#include "scene.h"
#include "systems.h"
#include "physics.h"
#include "primitives.h"
#include "components.h"
#include "application.h"

namespace Raekor {

RTTI_DEFINE_TYPE_NO_FACTORY(RandomWidget) {}

RandomWidget::RandomWidget(Application* inApp) :
	IWidget(inApp, " Random ")
{
}

void RandomWidget::Draw(Widgets* inWidgets, float dt)
{
	auto& scene = IWidget::GetScene();
	auto& render_settings = m_Editor->GetRenderInterface()->GetSettings();

	ImGui::Begin(m_Title.c_str(), &m_Open);
	m_Visible = ImGui::IsWindowAppearing();
	ImGui::SetItemDefaultFocus();

	if (ImGui::Checkbox("VSync", (bool*)( &GetRenderInterface().GetSettings().vsync )))
		SDL_GL_SetSwapInterval(GetRenderInterface().GetSettings().vsync);

	ImGui::SameLine();

	auto debug_physics = GetPhysics().GetDebugRendering();
	if (ImGui::Checkbox("Debug Physics", &debug_physics))
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

	if (ImGui::Button("Lower Spot Lights"))
	{
		for (const auto& [entity, light] : GetScene().Each<Light>())
		{
			if (light.type == LIGHT_TYPE_SPOT)
			{
				light.colour.a /= 1000.0f;
			}
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("Generate Meshlets"))
	{
		Timer timer; 

		m_Editor->LogMessage("Generating Meshlets..");

		for (const auto& [entity, mesh] : GetScene().Each<Mesh>())
		{
			const size_t max_vertices = 64;
			const size_t max_triangles = 124;
			const float cone_weight = 0.0f;

			size_t max_meshlets = meshopt_buildMeshletsBound(mesh.indices.size(), max_vertices, max_triangles);
			std::vector<meshopt_Meshlet> meshlets(max_meshlets);
			std::vector<unsigned int> meshlet_vertices(max_meshlets * max_vertices);
			std::vector<unsigned char> meshlet_triangles(max_meshlets * max_triangles * 3);

			size_t meshlet_count = meshopt_buildMeshlets(meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(), mesh.indices.data(),
				mesh.indices.size(), &mesh.positions[0].x, mesh.positions.size(), sizeof(mesh.positions[0]), max_vertices, max_triangles, cone_weight);
		
			meshlets.resize(meshlet_count);
			meshlet_vertices.resize(meshlet_count * max_vertices);
			meshlet_triangles.resize(meshlet_count* max_triangles * 3);
			
			mesh.meshlets.reserve(meshlet_count);
			mesh.meshletIndices.resize(meshlet_count * max_vertices);
			mesh.meshletTriangles.resize(meshlet_count * max_triangles);

			for (const auto& meshlet : meshlets)
			{
				auto& m = mesh.meshlets.emplace_back();
				m.mTriangleCount = meshlet.triangle_count;
				m.mTriangleOffset = meshlet.triangle_offset;
				m.mVertexCount = meshlet.vertex_count;
				m.mVertexOffset = meshlet.vertex_offset;
			}

			assert(mesh.meshletIndices.size() == meshlet_vertices.size());
			std::memcpy(mesh.meshletIndices.data(), meshlet_vertices.data(), mesh.meshletIndices.size());

			for (uint32_t idx = 0; idx < mesh.meshletTriangles.size(); idx += 3)
			{
				mesh.meshletTriangles[idx].mX = meshlet_triangles[idx];
				mesh.meshletTriangles[idx].mY = meshlet_triangles[idx + 1];
				mesh.meshletTriangles[idx].mZ = meshlet_triangles[idx + 2];
			}
		}

		m_Editor->LogMessage(std::format("Generating Meshlets took {:.3f} seconds.", timer.GetElapsedTime()));
	}

	// Draw all the renderer debug UI
	m_Editor->GetRenderInterface()->DrawImGui(GetScene(), m_Editor->GetViewport());

	ImGui::End();
}

} // raekor

