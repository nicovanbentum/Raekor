#include "PCH.h"
#include "../Engine/RTTI.h"
#include "../Engine/Script.h"
#include "../Engine/Physics.h"
#include "../Engine/Renderer/Device.h"
#include "../Engine/Renderer/Renderer.h"
#include "../Engine/Renderer/CommandList.h"
#include "../Engine/Renderer/RayTracedScene.h"

using namespace RK;

DECLARE_SCRIPT_CLASS(TestScript);
DECLARE_SCRIPT_CLASS(LightsScript);
DECLARE_SCRIPT_CLASS(AnimateSunScript);
DECLARE_SCRIPT_CLASS(VehicleControllerScript);
DECLARE_SCRIPT_CLASS(CharacterControllerScript);

void gRegisterScriptTypes()
{
    g_RTTIFactory.Register<TestScript>();
    g_RTTIFactory.Register<LightsScript>();
    g_RTTIFactory.Register<AnimateSunScript>();
    g_RTTIFactory.Register<VehicleControllerScript>();
    g_RTTIFactory.Register<CharacterControllerScript>();
}

class GameApp : public Application
{
public:
    GameApp() : 
        Application(WindowFlag::RESIZE),
        m_Device(this),
        m_Renderer(m_Device, m_Viewport, m_Window),
        m_RayTracedScene(m_Scene),
        m_RenderInterface(this, m_Device, m_Renderer, m_Renderer.GetRenderGraph().GetResources()),
        m_Scene(&m_RenderInterface),
        m_Physics(&m_RenderInterface)
    {
    }

    ~GameApp()
    {
        gRegisterScriptTypes();
    }

    void OnUpdate(float inDeltaTime) override
    {
        // update the physics system
        m_Physics.OnUpdate(m_Scene);

        // step the physics simulation
        if (m_Physics.GetState() == Physics::Stepping)
            m_Physics.Step(m_Scene, inDeltaTime);

        // update Transform components
        m_Scene.UpdateTransforms();

        // update camera transforms
        m_Scene.UpdateCameras();

        // update Light and DirectionalLight components
        m_Scene.UpdateLights();

        // update Skeleton and Animation components
        m_Scene.UpdateAnimations(inDeltaTime);

        // update NativeScript components
        if (GetGameState() == GAME_RUNNING)
            m_Scene.UpdateNativeScripts(inDeltaTime);

        for (const auto& [entity, mesh, skeleton] : m_Scene.Each<Mesh, Skeleton>())
            m_Renderer.QueueBlasUpdate(entity);
             
        m_Renderer.OnRender(this, m_Device, m_Viewport, m_RayTracedScene, GetRenderInterface(), inDeltaTime);
        
        m_Device.OnUpdate();
    }

    void OnEvent(const SDL_Event& inEvent) override
    {
        if (inEvent.type == SDL_WINDOWEVENT)
        {
            if (inEvent.window.event == SDL_WINDOWEVENT_MINIMIZED)
            {
                for (;;)
                {
                    SDL_Event temp_event;
                    SDL_PollEvent(&temp_event);

                    if (temp_event.window.event == SDL_WINDOWEVENT_RESTORED)
                        break;
                }
            }

            if (inEvent.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                if (!m_ConfigSettings.mShowUI)
                {
                    int w, h;
                    SDL_GetWindowSize(m_Window, &w, &h);
                    m_Viewport.SetDisplaySize(UVec2(w, h));
                }
            }
        }

        if (GetGameState() == GAME_RUNNING)
        {
            for (auto [entity, script] : m_Scene.Each<NativeScript>())
            {
                if (script.script)
                {
                    try
                    {
                        script.script->OnEvent(inEvent);
                    }
                    catch (const std::exception& e)
                    {
                        std::cerr << e.what() << '\n';
                    }
                }
            }
        }

        if (inEvent.type == SDL_WINDOWEVENT && inEvent.window.event == SDL_WINDOWEVENT_RESIZED)
            m_Renderer.SetShouldResize(true);
    }

    Scene* GetScene() override { return &m_Scene; }
    Assets* GetAssets() override { return &m_Assets; }
    Physics* GetPhysics() override { return &m_Physics; }

private:
    DX12::Device m_Device;
    DX12::Renderer m_Renderer;
    DX12::RayTracedScene m_RayTracedScene;
    DX12::RenderInterface m_RenderInterface;
    
    Scene m_Scene;
    Assets m_Assets;
    Physics m_Physics;
};

#if 0

int main(int argc, char** argv)
{
    g_CVariables = new CVariables(argc, argv);

    Application* app = new GameApp();

    app->Run();

    delete app;

    return 0;
}

#endif

