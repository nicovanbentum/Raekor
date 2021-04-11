#pragma once

#include "scene.h"
#include "application.h"
#include "renderer.h"

namespace Raekor {
namespace gui {

class ConsoleWindow {
public:
    ConsoleWindow();
    ~ConsoleWindow();

    static char* Strdup(const char* str);
    static void Strtrim(char* str);

    void ClearLog();

    // TODO: figure this out 
    void AddLog(const char* fmt, ...) IM_FMTARGS(2) {
        // FIXME-OPT
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
        buf[IM_ARRAYSIZE(buf) - 1] = 0;
        va_end(args);
		Items.insert(Items.begin(), Strdup(buf));
    }

    void    Draw(const char* title, bool* p_open);
    void    ExecCommand(const char* command_line);

    static int TextEditCallbackStub(ImGuiInputTextCallbackData* data);
    int TextEditCallback(ImGuiInputTextCallbackData* data);

private:
    char                  InputBuf[256];
    ImVector<char*>       Items;
    ImVector<char*>       History;
    int                   HistoryPos;
    ImGuiTextFilter       Filter;
    bool                  AutoScroll;
    bool                  ScrollToBottom;
};

void setTheme(const std::array<std::array<float, 4>, ImGuiCol_COUNT>& data);

//////////////////////////////////////////////////////////////////////////////////////////////////

void setFont(const std::string& filepath);

//////////////////////////////////////////////////////////////////////////////////////////////////

glm::ivec2 getMousePosWindow(const Viewport& viewport, ImVec2 windowPos);

//////////////////////////////////////////////////////////////////////////////////////////////////

class TopMenuBar {
public:
	void draw(WindowApplication* app, Scene& scene, GLRenderer& renderer, AssetManager& assetManager, entt::entity& active);
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class Dockspace {
public:
    void begin();
    void end();
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class InspectorWindow {
public:
    InspectorWindow(AssetManager& assetManager) : assetManager(assetManager) {}
    void draw(entt::registry& scene, entt::entity& entity);

private:
    void drawComponent(ecs::NameComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::NodeComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::MeshComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::MaterialComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::TransformComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::PointLightComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::NativeScriptComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::MeshAnimationComponent& component, entt::registry& scene, entt::entity& active);
    void drawComponent(ecs::DirectionalLightComponent& component, entt::registry& scene, entt::entity& active);

private:
    AssetManager& assetManager;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class EntityWindow {
public:
    void draw(entt::registry& scene, entt::entity& active);

private:
    void drawFamily(entt::registry& scene, entt::entity parent, entt::entity& active);
    bool drawFamilyNode(entt::registry& scene, entt::entity entity, entt::entity& active);
    void drawChildlessNode(entt::registry& scene, entt::entity entity, entt::entity& active);

    entt::entity active;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class Guizmo {
    friend class ViewportWindow;

public:
    void drawWindow();
    ImGuizmo::OPERATION getOperation();

private:
    bool enabled = true;
    ImGuizmo::OPERATION operation = ImGuizmo::OPERATION::TRANSLATE;
    std::array<const char*, 3> previews = { "TRANSLATE", "ROTATE", "SCALE"};
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class ViewportWindow {
public:
    bool draw(Viewport& viewport, GLRenderer& renderer, entt::registry& scene, entt::entity& active);
    void setTexture(unsigned int texture);

private:
    void drawGizmo(const Guizmo& gizmo, entt::registry& scene, Viewport& viewport, entt::entity active);

private:
    Guizmo gizmo;
    bool mouseInViewport;
    unsigned int texture;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class AssetWindow {
public:
    void drawWindow(entt::registry& assets, entt::entity& active);
};

class CameraSettings {
public:
    void drawWindow(Camera& camera);
};

class RandomWindow {
public:
	void drawWindow(GLRenderer& renderer, ViewportWindow& window);

private:
};

class PostprocessWindow {
public:
    void drawWindow(GLRenderer& renderer);
};

} // gui
} // raekor