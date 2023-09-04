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
        SDL_SetWindowTitle(m_Window, std::string(m_Settings.mSceneFile + " - Raekor Editor").c_str());
        m_Scene.OpenFromFile(m_Assets, m_Settings.mSceneFile);
        LogMessage("[Editor] Loaded scene from file: " + m_Settings.mSceneFile);
    }
}


void GLApp::OnUpdate(float inDeltaTime)
{
    IEditor::OnUpdate(inDeltaTime);

    // add debug geometry around a selected mesh
    if (GetActiveEntity() != NULL_ENTITY && m_Scene.Has<Mesh>(m_ActiveEntity))
    {
        const auto& [mesh, transform] = m_Scene.Get<Mesh, Transform>(m_ActiveEntity);
        m_Renderer.AddDebugBox(mesh.aabb[0], mesh.aabb[1], transform.worldTransform);
    }

    for (const auto& [entity, transform, mesh, soft_body] : m_Scene.Each<Transform, Mesh, SoftBody>())
        m_Renderer.UploadMeshBuffers(mesh);

    // render scene
    m_Renderer.Render(m_Scene, m_Viewport);
}


void GLApp::OnEvent(const SDL_Event& inEvent)
{
    IEditor::OnEvent(inEvent);

    if (inEvent.window.event == SDL_WINDOWEVENT_RESIZED)
        m_Renderer.CreateRenderTargets(m_Viewport);
}

} // Raekor::GL