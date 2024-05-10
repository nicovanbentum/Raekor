#include "pch.h"
#include "app.h"
#include "OS.h"
#include "archive.h"

namespace RK::GL {

GLApp::GLApp() :
    IEditor(WindowFlag::OPENGL | WindowFlag::RESIZE, &m_Renderer),
    m_Renderer(m_Window, m_Viewport)
{
    String scene_file = OS::sGetCommandLineValue("-scene_file");

    if (!fs::exists(m_Settings.mSceneFile) || !(Path(m_Settings.mSceneFile).extension() == ".scene"))
        scene_file = m_Settings.mSceneFile.string();

    if (fs::exists(scene_file) && Path(scene_file).extension() == ".scene")
    {
        SDL_SetWindowTitle(m_Window, std::string(m_Settings.mSceneFile.string() + " - RK Editor").c_str());
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