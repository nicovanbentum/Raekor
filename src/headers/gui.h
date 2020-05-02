#pragma once

#include "ecs.h"

namespace Raekor {
namespace GUI {

class InspectorWindow {
public:
    void draw(ECS::Scene& scene, ECS::Entity entity);

private:

    void drawNameComponent(ECS::NameComponent* component);
    void drawTransformComponent(ECS::TransformComponent* component);
    void drawMeshComponent(ECS::MeshComponent* component);
    void drawMeshRendererComponent(ECS::MeshRendererComponent* component);
    void drawMaterialComponent(ECS::MaterialComponent* component);
};

class EntityWindow {
public:
    void draw(ECS::Scene& scene, ECS::Entity& active) {
        ImGui::Begin("Entity Component System");
        auto treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen;
        if (ImGui::TreeNodeEx("Entities", treeNodeFlags)) {
            ImGui::Columns(1, NULL, false);
            unsigned int index = 0;
            for (uint32_t i = 0; i < scene.names.getCount(); i++) {
                ECS::Entity entity = scene.names.getEntity(i);
                bool selected = active == entity;
                std::string& name = scene.names.getComponent(entity)->name;
                if (ImGui::Selectable(std::string(name + "##" + std::to_string(index)).c_str(), selected)) {
                    active = entity;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
                index += 1;
            }
            ImGui::TreePop();
        }

        ImGui::End();
    }

};

/*
    Copied over from the ImGui Demo file, will be used for executing chaiscript evals 
*/
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

    void    Draw(const char* title, bool* p_open, chaiscript::ChaiScript& chai);
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

} // gui
} // raekor