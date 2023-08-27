#include "pch.h"
#include "randomWidget.h"
#include "timer.h"
#include "rmath.h"
#include "systems.h"
#include "physics.h"
#include "primitives.h"
#include "components.h"
#include "application.h"

namespace Raekor {

RTTI_CLASS_CPP_NO_FACTORY(RandomWidget) {}

RandomWidget::RandomWidget(Application* inApp) :
    IWidget(inApp, " Random ")
{}

void RandomWidget::Draw(Widgets* inWidgets, float dt) {
    auto& scene = IWidget::GetScene();
    auto& render_settings = m_Editor->GetRenderer()->GetSettings();

    ImGui::Begin(m_Title.c_str(), &m_Open);
    m_Visible = ImGui::IsWindowAppearing();
    ImGui::SetItemDefaultFocus();

    if (ImGui::Button("Generate Rigid Bodies")) {
        Timer timer;
        for (const auto& [sb_entity, sb_transform, sb_mesh] : scene.Each<Transform, Mesh>()) {
            if (!scene.Has<SoftBody>(sb_entity))
                scene.Add<BoxCollider>(sb_entity);
        }

        GetPhysics().GenerateRigidBodiesEntireScene(GetScene());

        g_ThreadPool.WaitForJobs();
        m_Editor->LogMessage("Rigid Body Generation took " + timer.GetElapsedFormatted() + " seconds.");
    }

    if (ImGui::Button("Spawn/Reset Balls")) {
        const auto material_entity = scene.Create();
        auto& material_name = scene.Add<Name>(material_entity);
        material_name = "Ball Material";
        auto& ball_material = scene.Add<Material>(material_entity);
        ball_material.albedo = glm::vec4(1.0f, 0.25f, 0.38f, 1.0f);

        if (auto renderer = m_Editor->GetRenderer())
            renderer->UploadMaterialTextures(ball_material, GetAssets());

        for (uint32_t i = 0; i < 64; i++) {
            auto entity = scene.CreateSpatialEntity("ball");
            auto& transform = scene.Get<Transform>(entity);
            auto& mesh = scene.Add<Mesh>(entity);

            mesh.material = material_entity;
            
            constexpr auto radius = 2.5f;
            gGenerateSphere(mesh, radius, 32, 32);
            GetRenderer().UploadMeshBuffers(mesh);

            transform.position = Vec3(-65.0f, 85.0f + i * (radius * 2.0f), 0.0f);
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

    auto debug_physics = GetPhysics().GetDebugRendering();
    if (ImGui::Checkbox("Debug Physics", &debug_physics))
        GetPhysics().SetDebugRendering(debug_physics);

    // Draw all the renderer debug UI
    m_Editor->GetRenderer()->DrawImGui(GetScene(), m_Editor->GetViewport());

    ImGui::End();
}

} // raekor

