#include "pch.h"
#include "app.h"
#include "archive.h"

namespace Raekor::GL {

GLApp::GLApp() :
    IEditor(WindowFlag::OPENGL | WindowFlag::RESIZE, &m_Renderer),
    m_Renderer(m_Window, m_Viewport)
{
    if (fs::exists(m_Settings.mSceneFile) && Path(m_Settings.mSceneFile).extension() == ".scene")
    {
        SDL_SetWindowTitle(m_Window, std::string(m_Settings.mSceneFile.string() + " - Raekor Editor").c_str());
        m_Scene.OpenFromFile(m_Settings.mSceneFile.string(), m_Assets);
        LogMessage("[Editor] Loaded scene from file: " + m_Settings.mSceneFile.string());
    }
}


void GLApp::OnUpdate(float inDeltaTime)
{
    IEditor::OnUpdate(inDeltaTime);

    // add debug geometry around a selected mesh
    if (GetActiveEntity() != NULL_ENTITY)
    {
        if (m_Scene.Has<Mesh>(m_ActiveEntity))
        {
            const auto& [mesh, transform] = m_Scene.Get<Mesh, Transform>(m_ActiveEntity);
            m_Renderer.AddDebugBox(mesh.aabb[0], mesh.aabb[1], transform.worldTransform);
        }

        else if (m_Scene.Has<Light>(m_ActiveEntity))
        {
            const auto& light = m_Scene.Get<Light>(m_ActiveEntity);

            if (light.type == LIGHT_TYPE_SPOT)
            {
                Vec3 pos = light.position;
                Vec3 target = pos + light.direction * light.attributes.x;

                Vec3 right = glm::normalize(glm::cross(light.direction, glm::vec3(0.0f, 1.0f, 0.0f)));
                Vec3 up = glm::cross(right, light.direction);

                // Calculate cone radius and step angle
                float half_angle = light.attributes.z / 2.0f;
                float cone_radius = light.attributes.x * tan(half_angle);

                constexpr auto steps = 16u;
                constexpr auto step_angle = glm::radians(360.0f / float(steps));

                Vec3 last_point;

                for (int i = 0; i <= steps; i++)
                {
                    float angle = step_angle * i;
                    Vec3 circle_point = target + cone_radius * ( cos(angle) * right + sin(angle) * up );

                    m_Renderer.AddDebugLine(pos, circle_point);

                    if (i > 0)
                        m_Renderer.AddDebugLine(last_point, circle_point);

                    last_point = circle_point;
                }
            }

            else if (light.type == LIGHT_TYPE_POINT)
            {
                const auto light_pos = Vec3(light.position);

                constexpr float step_size = M_PI * 2.0f / 32.0f;

                auto DrawDebugCircle = [&](Vec3 inDir, Vec3 inAxis)
                {
                    Vec3 point = inDir;

                    for (float angle = 0.0f; angle < M_PI * 2.0f; angle += step_size)
                    {
                        Vec3 new_point = glm::toMat3(glm::angleAxis(step_size, inAxis)) * point;

                        Vec3 start_point = light_pos + point * light.attributes.x;

                        Vec3 end_point = light_pos + new_point * light.attributes.x;

                        m_Renderer.AddDebugLine(start_point, end_point);

                        point = new_point;

                    }
                };

                DrawDebugCircle(Vec3(1, 0, 0), Vec3(0, 1, 0));
                DrawDebugCircle(Vec3(0, 1, 0), Vec3(0, 0, 1));
                DrawDebugCircle(Vec3(0, 0, 1), Vec3(1, 0, 0));
            }
        }
        else if (m_Scene.Has<DirectionalLight>(GetActiveEntity()))
        {
            const DirectionalLight& light = m_Scene.Get<DirectionalLight>(m_ActiveEntity);
            Vec3 direction = Vec3(light.GetDirection());

            if (m_Scene.Has<Transform>(GetActiveEntity()))
            {
                const Transform& transform = m_Scene.Get<Transform>(GetActiveEntity());

                constexpr float cWidth = 0.6f;
                constexpr float cLength = 2.4f;

                Vec3 start_point = transform.GetPositionWorldSpace();
                Vec3 end_point = start_point + direction * 4.0f;

                for (int i = 0; i < 4; i++)
                {
                    constexpr float up_dirs[4] = { 1.0f, -1.0f, 0.0f, 0.0f };
                    constexpr float right_dirs[4] = { 0.0f, 0.0f, -1.0f, 1.0f };


                    Vec3 right = glm::normalize(glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f)));
                    Vec3 up = glm::normalize(glm::cross(right, direction));

                    up = up * up_dirs[i];
                    right = right * right_dirs[i];

                    Vec3 curr_point = start_point;
                    Vec3 next_point = start_point + up * cWidth + right * cWidth;
                    m_Renderer.AddDebugLine(curr_point, next_point);

                    curr_point = next_point;
                    next_point = curr_point + direction * cLength;
                    m_Renderer.AddDebugLine(curr_point, next_point);


                    curr_point = next_point;
                    next_point = curr_point + up * cWidth + right * cWidth;
                    m_Renderer.AddDebugLine(curr_point, next_point);

                    m_Renderer.AddDebugLine(next_point, end_point);
                }
            }
        }
    }

    constexpr auto cInfiniteLineLength = 4096.0f;
    m_Renderer.AddDebugLine(Vec3(-cInfiniteLineLength, 0, 0), Vec3(cInfiniteLineLength, 0, 0));
    m_Renderer.AddDebugLine(Vec3(0, -cInfiniteLineLength, 0), Vec3(0, cInfiniteLineLength, 0));
    m_Renderer.AddDebugLine(Vec3(0, 0, -cInfiniteLineLength), Vec3(0, 0, cInfiniteLineLength));

    for (const auto& [entity, transform, mesh, soft_body] : m_Scene.Each<Transform, Mesh, SoftBody>())
        m_Renderer.UploadMeshBuffers(entity, mesh);

    // render scene
    m_Renderer.Render(this, m_Scene, m_Viewport);
}


void GLApp::OnEvent(const SDL_Event& inEvent)
{
    IEditor::OnEvent(inEvent);

    if (inEvent.window.event == SDL_WINDOWEVENT_RESIZED)
    {
        m_Viewport.SetRenderSize(m_Viewport.GetDisplaySize());
        m_Renderer.CreateRenderTargets(m_Viewport);
    }
}

} // Raekor::GL