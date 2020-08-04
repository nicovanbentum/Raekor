#pragma once

#include "ecs.h"
#include "camera.h"

#include "scene.h"
#include "components.h"

namespace Raekor {
namespace gui {

class InspectorWindow {
public:
    void draw(entt::registry& scene, entt::entity& entity);

private:
    void drawNameComponent                  (ecs::NameComponent& component);
    void drawNodeComponent                  (ecs::NodeComponent& component);
    void drawMeshComponent                  (ecs::MeshComponent& component, entt::registry& scene, entt::entity& active);
    void drawMaterialComponent              (ecs::MaterialComponent& component);
    void drawTransformComponent             (ecs::TransformComponent& component);
    void drawPointLightComponent            (ecs::PointLightComponent& component);
    void drawAnimationComponent             (ecs::MeshAnimationComponent& component);
    void drawDirectionalLightComponent      (ecs::DirectionalLightComponent& component);
};

class EntityWindow {
public:
    void draw(entt::registry& scene, entt::entity& active);

private:
    void drawFamily(entt::registry& scene, entt::entity parent, entt::entity& active);
    bool drawFamilyNode(entt::registry& scene, entt::entity entity, entt::entity& active);
    void drawChildlessNode(entt::registry& scene, entt::entity entity, entt::entity& active);
};


// Copied over from the ImGui Demo file, used for executing chaiscript evals 
class ConsoleWindow {
public:
    ConsoleWindow();
    ~ConsoleWindow();

    static char* Strdup(const char* str);
    static void Strtrim(char* str);

    void ClearLog();

    // TODO: figure this out 
    void AddLog(const char* fmt, ...) IM_FMTARGS(2)
    {
        // FIXME-OPT
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
        buf[IM_ARRAYSIZE(buf) - 1] = 0;
        va_end(args);
        Items.push_back(Strdup(buf));
    }

    void    Draw(chaiscript::ChaiScript* chai);
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

class Guizmo {
public:
    void drawGuizmo(entt::registry& scene, Viewport& viewport, entt::entity active);
    void drawWindow();
    inline ImGuizmo::OPERATION getOperation() { return operation; }

private:
    bool enabled = true;
    ImGuizmo::OPERATION operation = ImGuizmo::OPERATION::TRANSLATE;
    std::array<const char*, 3> previews = { "TRANSLATE", "ROTATE", "SCALE"};
};

class AssetBrowser {
public:
    void drawWindow(entt::registry& assets, entt::entity& active);
};

} // gui
} // raekor