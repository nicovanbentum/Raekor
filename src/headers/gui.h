#pragma once

#include "ecs.h"
#include "camera.h"

#include "scene.h"
#include "components.h"

namespace Raekor {
namespace GUI {

class InspectorWindow {
public:
    void draw(Scene& scene, ECS::Entity entity);

private:
    void drawNameComponent(ECS::NameComponent* component);
    void drawNodeComponent(ECS::NodeComponent* component);
    void drawMeshComponent(ECS::MeshComponent* component);
    void drawMaterialComponent(ECS::MaterialComponent* component);
    void drawTransformComponent(ECS::TransformComponent* component);
    void drawPointLightComponent(ECS::PointLightComponent* component);
    void drawDirectionalLightComponent(ECS::DirectionalLightComponent* component);
};

class EntityWindow {
public:
    void draw(Scene& scene, ECS::Entity& active);

private:
    void drawFamily(Scene& scene, ECS::Entity parent, ECS::Entity& active);
    bool drawFamilyNode(Scene& scene, ECS::Entity entity, ECS::Entity& active);
    void drawChildlessNode(Scene& scene, ECS::Entity entity, ECS::Entity& active);
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
    void drawGuizmo(Scene& scene, Viewport& viewport, ECS::Entity active);
    void drawWindow();

private:
    bool enabled = true;
    ImGuizmo::OPERATION operation = ImGuizmo::OPERATION::TRANSLATE;
    std::array<const char*, 3> previews = { "TRANSLATE", "ROTATE", "SCALE"};
};

} // gui
} // raekor