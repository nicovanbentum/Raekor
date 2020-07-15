#include "pch.h"
#include "gui.h"
#include "platform/OS.h"

namespace Raekor {
namespace GUI {

void InspectorWindow::draw(Scene& scene, ECS::Entity entity) {
    ImGui::Begin("Inspector");
    if (entity != NULL) {
        ImGui::Text("ID: %i", entity);
        if (scene.names.contains(entity)) {
            if (ImGui::CollapsingHeader("Name Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                drawNameComponent(scene.names.getComponent(entity));
            }
        }

        if (scene.nodes.contains(entity)) {
            if (ImGui::CollapsingHeader("Node Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                drawNodeComponent(scene.nodes.getComponent(entity));
            }
        }

        if (scene.transforms.contains(entity)) {
            if (ImGui::CollapsingHeader("Transform Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                drawTransformComponent(scene.transforms.getComponent(entity));
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

        if (scene.pointLights.contains(entity)) {
            bool isOpen = true; // for checking if the close button was clicked
            if (ImGui::CollapsingHeader("Point Light Component", &isOpen, ImGuiTreeNodeFlags_DefaultOpen)) {
                if (isOpen) drawPointLightComponent(scene.pointLights.getComponent(entity));
                else scene.materials.remove(entity);
            }
        }

        if (scene.directionalLights.contains(entity)) {
            bool isOpen = true; // for checking if the close button was clicked
            if (ImGui::CollapsingHeader("Directional Light Component", &isOpen, ImGuiTreeNodeFlags_DefaultOpen)) {
                if (isOpen) drawDirectionalLightComponent(scene.directionalLights.getComponent(entity));
                else scene.directionalLights.remove(entity);
            }
        }

        if (scene.animations.contains(entity)) {
            bool isOpen = true;
            if (ImGui::CollapsingHeader("Animation Component", &isOpen, ImGuiTreeNodeFlags_DefaultOpen)) {
                if (isOpen) drawAnimationComponent(scene.animations.getComponent(entity));
                else scene.animations.remove(entity);
            }
        }

        if (ImGui::BeginPopup("Components"))
        {
            if (ImGui::Selectable("Transform", false)) {
                scene.transforms.create(entity);
                ImGui::CloseCurrentPopup();
            }

            if(ImGui::Selectable("Mesh", false)) {
                scene.meshes.create(entity);
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::Selectable("Material", false)) {
                scene.materials.create(entity);
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::Selectable("Point Light", false)) {
                scene.pointLights.create(entity);
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::Selectable("Directional Light", false)) {
                scene.directionalLights.create(entity);
                ImGui::CloseCurrentPopup();
            }


            ImGui::EndPopup();
        }

        if (ImGui::Button("Add Component", ImVec2(ImGui::GetWindowWidth(), 0))) {
            ImGui::OpenPopup("Components");
        }
    }

    ImGui::End();
}

void InspectorWindow::drawNameComponent(ECS::NameComponent* component) {
    if (ImGui::InputText("Name", &component->name, ImGuiInputTextFlags_AutoSelectAll)) {
        if (component->name.size() > 16) {
            component->name = component->name.substr(0, 16);
        }
    }
}

void InspectorWindow::drawNodeComponent(ECS::NodeComponent* component) {
    ImGui::Text("Parent entity: %i", component->parent);
    ImGui::SameLine();
    ImGui::Text("| Has children: %s", component->hasChildren ? "True" : "False");
}

void InspectorWindow::drawTransformComponent(ECS::TransformComponent* component) {
    if (ImGui::DragFloat3("Scale", glm::value_ptr(component->scale))) {
        component->recalculateMatrix();
    }
    if (ImGui::DragFloat3("Rotation", glm::value_ptr(component->rotation), 0.001f, FLT_MIN, FLT_MAX)) {
        component->recalculateMatrix();
    }
    if (ImGui::DragFloat3("Position", glm::value_ptr(component->position), 0.001f, FLT_MIN, FLT_MAX)) {
        component->recalculateMatrix();
    }
}

void InspectorWindow::drawMeshComponent(ECS::MeshComponent* component) {
    ImGui::Text("Vertex count: %i", component->vertices.size());
    ImGui::Text("Index count: %i", component->indices.size() * 3);
}

void InspectorWindow::drawMaterialComponent(ECS::MaterialComponent* component) {
    Ffilter textureFileFormats;
    textureFileFormats.name = "Supported Image Files";
    textureFileFormats.extensions = "*.png;*.jpg;*.jpeg;*.tga";

    if (component->albedo) {
        ImGui::Image(component->albedo->ImGuiID(), ImVec2(15, 15));
        ImGui::SameLine();
    }

    ImGui::Text("Albedo");
    ImGui::SameLine();
    if (ImGui::SmallButton("... ## albedo")) {
        std::string filepath = OS::openFileDialog( { textureFileFormats } );
        if (!filepath.empty()) {
            Stb::Image image;
            image.load(filepath, true);

            if (!component->albedo) {
                component->albedo.reset(new glTexture2D());
            }

            component->albedo->bind();
            component->albedo->init(image.w, image.h, Format::SRGBA_U8, image.pixels);
            component->albedo->setFilter(Sampling::Filter::Trilinear);
            component->albedo->genMipMaps();
            component->albedo->unbind();
        }
    }

    if (component->normals) {
        ImGui::Image(component->normals->ImGuiID(), ImVec2(15, 15));
        ImGui::SameLine();
    }
    
    ImGui::Text("Normal map");
    ImGui::SameLine();
    if (ImGui::SmallButton("... ## normalmap")) {
        std::string filepath = OS::openFileDialog({ textureFileFormats });
        if (!filepath.empty()) {
            Stb::Image image;
            image.load(filepath, true);

            if (!component->normals) {
                component->normals.reset(new glTexture2D());
            }

            component->normals->bind();
            component->normals->init(image.w, image.h, Format::RGBA_U8, image.pixels);
            component->normals->setFilter(Sampling::Filter::None);
            component->normals->unbind();
        }
    }

    if (component->metalrough) {
        ImGui::Image(component->metalrough->ImGuiID(), ImVec2(15, 15));
        ImGui::SameLine();
    }

    ImGui::Text("Metallic Roughness map");
    ImGui::SameLine();
    if (ImGui::SmallButton("... ## metallic roughness")) {
        std::string filepath = OS::openFileDialog({ textureFileFormats });
    }

}

void InspectorWindow::drawPointLightComponent(ECS::PointLightComponent* component) {
    ImGui::ColorEdit4("Colour", glm::value_ptr(component->buffer.colour));
}

void InspectorWindow::drawDirectionalLightComponent(ECS::DirectionalLightComponent* component) {
    ImGui::ColorEdit4("Colour", glm::value_ptr(component->buffer.colour));
    ImGui::DragFloat3("Direction", glm::value_ptr(component->buffer.direction), 0.01f, -1.0f, 1.0f);
}

void InspectorWindow::drawAnimationComponent(ECS::MeshAnimationComponent* component) {
    static bool playing = false;
    if (ImGui::Button(playing ? "pause" : "play")) {
        playing = !playing;
    }

    ImGui::SliderFloat("Time", &component->animation.runningTime, 0, component->animation.totalDuration);
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

void ConsoleWindow::Draw(chaiscript::ChaiScript* chai)
{
    if (!ImGui::Begin("Console"))
    {
        ImGui::End();
        return;
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

            try {
                chai->eval(evaluation);
            } catch (const chaiscript::exception::eval_error& ee) {
                std::cout << ee.what();
                if (ee.call_stack.size() > 0) {
                    std::cout << "during evaluation at (" << ee.call_stack[0].start().line << ", " << ee.call_stack[0].start().column << ")";
                }
                std::cout << '\n';
            } catch (const std::exception& e) {
                std::cout << e.what() << '\n';
            }
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

bool EntityWindow::drawFamilyNode(Scene& scene, ECS::Entity entity, ECS::Entity& active) {
    auto selected = active == entity ? ImGuiTreeNodeFlags_Selected : 0;
    auto treeNodeFlags = selected | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow;
    auto name = scene.names.getComponent(entity);
    bool opened = ImGui::TreeNodeEx(name->name.c_str(), treeNodeFlags);
    if (ImGui::IsItemClicked()) {
        active = active == entity ? NULL : entity;
    }
     return opened;
}

void EntityWindow::drawChildlessNode(Scene& scene, ECS::Entity entity, ECS::Entity& active) {
    auto name = scene.names.getComponent(entity);
    if (ImGui::Selectable(std::string(name->name + "##" + std::to_string(entity)).c_str(), entity == active)) {
        active = active == entity ? NULL : entity;
    }
}

void EntityWindow::drawFamily(Scene& scene, ECS::Entity parent, ECS::Entity& active) {
    for (int i = 0; i < scene.nodes.getCount(); i++) {
        if (scene.nodes[i].parent == parent) {
            if (scene.nodes[i].hasChildren) {
                if (drawFamilyNode(scene, scene.nodes.getEntity(i), active)) {
                    drawFamily(scene, scene.nodes.getEntity(i), active);
                    ImGui::TreePop();
                }
            } else {
                drawChildlessNode(scene, scene.nodes.getEntity(i), active);
            }
        }
    }
}

void EntityWindow::draw(Scene& scene, ECS::Entity& active) {
    ImGui::Begin("Scene");

    for(int i = 0; i < scene.nodes.getCount(); i++) {
        if (scene.nodes[i].parent == NULL) {
            auto entity = scene.nodes.getEntity(i);
            if (scene.nodes.getComponent(entity)->hasChildren) {
                if (drawFamilyNode(scene, entity, active)) {
                    drawFamily(scene, entity, active);
                    ImGui::TreePop();
                }
            } else {
                drawChildlessNode(scene, entity, active);
            }
        }
    }

    ImGui::End();
}

void Guizmo::drawGuizmo(Scene& scene, Viewport& viewport, ECS::Entity active) {
    ECS::TransformComponent* transform = scene.transforms.getComponent(active);
    if (!active || !enabled || !transform) return;

    // set the gizmo's viewport
    ImGuizmo::SetDrawlist();
    auto pos = ImGui::GetWindowPos();
    ImGuizmo::SetRect(pos.x, pos.y, (float)viewport.size.x, (float)viewport.size.y);

    // temporarily transform to local space for gizmo use
    transform->matrix = glm::translate(transform->matrix, transform->localPosition);

    // draw gizmo
    ImGuizmo::Manipulate(
        glm::value_ptr(viewport.getCamera().getView()),
        glm::value_ptr(viewport.getCamera().getProjection()),
        operation, ImGuizmo::MODE::LOCAL,
        glm::value_ptr(transform->matrix)
    );

    // transform back to world space
    transform->matrix = glm::translate(transform->matrix, -transform->localPosition);

    // update the transformation
    ImGuizmo::DecomposeMatrixToComponents(
        glm::value_ptr(transform->matrix),
        glm::value_ptr(transform->position),
        glm::value_ptr(transform->rotation),
        glm::value_ptr(transform->scale)
    );

    transform->rotation = glm::radians(transform->rotation);
}

void Guizmo::drawWindow() {
    ImGui::Begin("Editor");
    if (ImGui::Checkbox("Gizmo", &enabled)) {
        ImGuizmo::Enable(enabled);
    }

    ImGui::Separator();

    if (ImGui::RadioButton("Move", operation == ImGuizmo::OPERATION::TRANSLATE)) {
        operation = ImGuizmo::OPERATION::TRANSLATE;
    }

    ImGui::SameLine();

    if (ImGui::RadioButton("Rotate", operation == ImGuizmo::OPERATION::ROTATE)) {
        operation = ImGuizmo::OPERATION::ROTATE;
    }

    ImGui::SameLine();


    if (ImGui::RadioButton("Scale", operation == ImGuizmo::OPERATION::SCALE)) {
        operation = ImGuizmo::OPERATION::SCALE;
    }

    ImGui::End();
}

} // gui
} // raekor