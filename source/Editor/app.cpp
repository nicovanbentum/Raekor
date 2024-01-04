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