#include "pch.h"
#include "randomWidget.h"
#include "editor.h"
#include "renderer.h"
#include "Raekor/systems.h"

namespace Raekor {

RTTI_CLASS_CPP_NO_FACTORY(RandomWidget) {}

RandomWidget::RandomWidget(Editor* editor) :
    IWidget(editor, " Random ")
{}

void RandomWidget::draw(float dt) {
    auto& scene = IWidget::GetScene();
    auto& renderer = IWidget::GetRenderer();

    ImGui::Begin(m_Title.c_str(), &m_Visible);
    ImGui::SetItemDefaultFocus();

    if (ImGui::Checkbox("VSync", (bool*)(&renderer.settings.vsync))) {
        SDL_GL_SetSwapInterval(renderer.settings.vsync);
    }

    ImGui::SameLine();

    if (ImGui::Checkbox("TAA", (bool*)(&renderer.settings.enableTAA))) {
        renderer.m_FrameNr = 0;
    }

    if (ImGui::Button("Generate Rigid Bodies")) {
        Timer timer;
        GetPhysics().GenerateRigidBodiesEntireScene(GetScene());
        std::cout << "Rigid Body Generation took " <<  Timer::sToMilliseconds(timer.GetElapsedTime()) << " ms.\n";
    }

    if (ImGui::Button("Spawn/Reset Balls")) {
        const auto material_entity = scene.create();
        auto& material_name = scene.emplace<Name>(material_entity);
        material_name = "Ball Material";
        auto& ball_material = scene.emplace<Material>(material_entity);
        ball_material.albedo = glm::vec4(1.0f, 0.25f, 0.38f, 1.0f);
        GLRenderer::sUploadMaterialTextures(ball_material, GetAssets());

        for (uint32_t i = 0; i < 64; i++) {
            auto entity = scene.CreateSpatialEntity("ball");
            auto& transform = scene.get<Transform>(entity);
            auto& mesh = scene.emplace<Mesh>(entity);

            mesh.material = material_entity;
            constexpr auto radius = 2.5f;
            gGenerateSphere(mesh, radius, 16, 16);
            GLRenderer::sUploadMeshBuffers(mesh);

            transform.position = Vec3(-65.0f, 85.0f + i * (radius * 2.0f), 0.0f);
            transform.Compose();

            auto& collider = scene.emplace<BoxCollider>(entity);
            collider.motionType = JPH::EMotionType::Dynamic;
            JPH::Ref<JPH::ShapeSettings> settings = new JPH::SphereShapeSettings(radius);

            auto body_settings = JPH::BodyCreationSettings(
                settings,
                JPH::Vec3(transform.position.x, transform.position.y, transform.position.z),
                JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w),
                collider.motionType,
                EPhysicsObjectLayers::MOVING
            );

            body_settings.mFriction = 0.1;
            body_settings.mRestitution = 0.35;

            auto& body_interface = GetPhysics().GetSystem().GetBodyInterface();
            collider.bodyID = body_interface.CreateAndAddBody(body_settings, JPH::EActivation::Activate);

            //body_interface.AddImpulse(collider.bodyID, JPH::Vec3Arg(50.0f, -2.0f, 50.0f));
        }
    }

    ImGui::NewLine(); 
    ImGui::Text("VCTGI");
    ImGui::Separator();

    ImGui::DragFloat("Radius", &renderer.m_Voxelize->worldSize, 0.05f, 1.0f, FLT_MAX, "%.2f");

    ImGui::NewLine(); 
    ImGui::Text("CSM");
    ImGui::Separator();

    if (ImGui::DragFloat("Bias constant", &renderer.m_ShadowMaps->settings.depthBiasConstant, 0.01f, 0.0f, FLT_MAX, "%.2f")) {}
    if (ImGui::DragFloat("Bias slope factor", &renderer.m_ShadowMaps->settings.depthBiasSlope, 0.01f, 0.0f, FLT_MAX, "%.2f")) {}
    if (ImGui::DragFloat("Split lambda", &renderer.m_ShadowMaps->settings.cascadeSplitLambda, 0.0001f, 0.0f, 1.0f, "%.4f")) {
        renderer.m_ShadowMaps->updatePerspectiveConstants(m_Editor->GetViewport());
    }

    ImGui::NewLine(); 
    ImGui::Text("Bloom");
    ImGui::Separator();
    ImGui::DragFloat3("Threshold", glm::value_ptr(renderer.m_DeferredShading->settings.bloomThreshold), 0.01f, 0.0f, 10.0f, "%.3f");

    ImGui::End();
}

} // raekor

