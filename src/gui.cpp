#include "pch.h"
#include "gui.h"

namespace Raekor {
namespace GUI {

void InspectorWindow::draw(ECS::Scene& scene, ECS::Entity entity) {
    ImGui::Begin("Inspector");
    if (entity != NULL) {
        ImGui::Text("ID: %i", entity);
        if (scene.names.contains(entity)) {
            if (ImGui::CollapsingHeader("Name Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                drawNameComponent(scene.names.getComponent(entity));
            }
        }

        if (scene.transforms.contains(entity)) {
            bool isOpen = true; // for checking if the close button was clicked
            if (ImGui::CollapsingHeader("Transform Component", &isOpen, ImGuiTreeNodeFlags_DefaultOpen)) {
                if (isOpen) drawTransformComponent(scene.transforms.getComponent(entity));
                else scene.transforms.remove(entity);
            }
        }

        if (scene.meshes.contains(entity)) {
            bool isOpen = true; // for checking if the close button was clicked
            if (ImGui::CollapsingHeader("Mesh Component", &isOpen, ImGuiTreeNodeFlags_DefaultOpen)) {
                if (isOpen) drawMeshComponent(scene.meshes.getComponent(entity));
                else scene.meshes.remove(entity);
            }
        }

        if (scene.materials.contains(entity)) {
            bool isOpen = true; // for checking if the close button was clicked
            if (ImGui::CollapsingHeader("Material Component", &isOpen, ImGuiTreeNodeFlags_DefaultOpen)) {
                if (isOpen) drawMaterialComponent(scene.materials.getComponent(entity));
                else scene.materials.remove(entity);
            }
        }
    }

    ImGui::End();
}

void InspectorWindow::drawNameComponent(ECS::NameComponent* component) {
    if (ImGui::InputText("Name", &component->name, ImGuiInputTextFlags_AutoSelectAll)) {
        if (component->name.size() > 8) {
            component->name = component->name.substr(0, 12);
        }
    }
}

void InspectorWindow::drawTransformComponent(ECS::TransformComponent* component) {
    if (ImGui::DragFloat3("Scale", glm::value_ptr(component->scale))) {}
    if (ImGui::DragFloat3("Rotation", glm::value_ptr(component->rotation))) {}
    if (ImGui::DragFloat3("Position", glm::value_ptr(component->position))) {}
}

void InspectorWindow::drawMeshComponent(ECS::MeshComponent* component) {
    ImGui::Text("Vertex count: %i", component->vertices.size());
    ImGui::Text("Index count: %i", component->indices.size() * 3);
}

void InspectorWindow::drawMeshRendererComponent(ECS::MeshRendererComponent* component) {

}

void InspectorWindow::drawMaterialComponent(ECS::MaterialComponent* component) {
    ImGui::Image(component->albedo->ImGuiID(), ImVec2(50, 50));
    ImGui::SameLine();
    ImGui::Text("Albedo");

    if (component->normals.get() != nullptr) {
        ImGui::Image(component->normals->ImGuiID(), ImVec2(50, 50));
        ImGui::SameLine();
        ImGui::Text("Normal map");
    }
}

ConsoleWindow::ConsoleWindow() {
    ClearLog();
    memset(InputBuf, 0, sizeof(InputBuf));
    HistoryPos = -1;
    AutoScroll = true;
    ScrollToBottom = false;
}

ConsoleWindow::~ConsoleWindow()
{
    ClearLog();
    for (int i = 0; i < History.Size; i++)
        free(History[i]);
}

char* ConsoleWindow::Strdup(const char* str) { size_t len = strlen(str) + 1; void* buf = malloc(len); IM_ASSERT(buf); return (char*)memcpy(buf, (const void*)str, len); }
void  ConsoleWindow::Strtrim(char* str) { char* str_end = str + strlen(str); while (str_end > str && str_end[-1] == ' ') str_end--; *str_end = 0; }

void ConsoleWindow::ClearLog()
{
    for (int i = 0; i < Items.Size; i++)
        free(Items[i]);
    Items.clear();
}

void    ConsoleWindow::Draw(const char* title, bool* p_open, chaiscript::ChaiScript& chai)
{
    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(title, p_open))
    {
        ImGui::End();
        return;
    }

    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Close Console"))
            *p_open = false;
        ImGui::EndPopup();
    }

    ImGui::Separator();

    const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);
    if (ImGui::BeginPopupContextWindow())
    {
        if (ImGui::Selectable("Clear")) ClearLog();
        ImGui::EndPopup();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
    for (int i = 0; i < Items.Size; i++)
    {
        const char* item = Items[i];
        if (!Filter.PassFilter(item))
            continue;

        ImGui::TextUnformatted(item);
    }

    if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
        ImGui::SetScrollHereY(1.0f);
    ScrollToBottom = false;

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::Separator();

    // Command-line
    bool reclaim_focus = false;
    if (ImGui::InputText("##Input", InputBuf, IM_ARRAYSIZE(InputBuf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory, &TextEditCallbackStub, (void*)this))
    {
        char* s = InputBuf;
        Strtrim(s);
        if (s[0]) {
            ExecCommand(s);
            std::string evaluation = std::string(s);
            chai.eval(evaluation);
        }
        strcpy(s, "");
        reclaim_focus = true;
    }

    // Auto-focus on window apparition
    ImGui::SetItemDefaultFocus();
    if (reclaim_focus)
        ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) { ClearLog(); }

    ImGui::End();
}

void    ConsoleWindow::ExecCommand(const char* command_line)
{
    AddLog(command_line);

    // On command input, we scroll to bottom even if AutoScroll==false
    ScrollToBottom = true;
}

int ConsoleWindow::TextEditCallbackStub(ImGuiInputTextCallbackData* data) // In C++11 you are better off using lambdas for this sort of forwarding callbacks
{
    ConsoleWindow* console = (ConsoleWindow*)data->UserData;
    return console->TextEditCallback(data);
}

int ConsoleWindow::TextEditCallback(ImGuiInputTextCallbackData* data) { return 0; }

} // gui
} // raekor