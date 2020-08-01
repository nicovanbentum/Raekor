#include "pch.h"
#include "gui.h"
#include "platform/OS.h"

namespace Raekor {
namespace gui {

void InspectorWindow::draw(entt::registry& scene, entt::entity entity) {
    ImGui::Begin("Inspector");
    if (entity != entt::null) {
        ImGui::Text("ID: %i", entity);
        if (scene.has<ecs::NameComponent>(entity)) {
            if (ImGui::CollapsingHeader("Name Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                drawNameComponent(scene.get<ecs::NameComponent>(entity));
            }
        }

        if (scene.has<ecs::NodeComponent>(entity)) {
            if (ImGui::CollapsingHeader("Node Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                drawNodeComponent(scene.get<ecs::NodeComponent>(entity));
            }
        }

        if (scene.has<ecs::TransformComponent>(entity)) {
            if (ImGui::CollapsingHeader("Transform Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                drawTransformComponent(scene.get<ecs::TransformComponent>(entity));
            }
        }

        if (scene.has<ecs::MeshComponent>(entity)) {
            bool isOpen = true; // for checking if the close button was clicked
            if (ImGui::CollapsingHeader("Mesh Component", &isOpen, ImGuiTreeNodeFlags_DefaultOpen)) {
                if (isOpen) drawMeshComponent(scene.get<ecs::MeshComponent>(entity), scene);
                else scene.remove<ecs::MeshComponent>(entity);
            }
        }

        if (scene.has<ecs::MaterialComponent>(entity)) {
            bool isOpen = true; // for checking if the close button was clicked
            if (ImGui::CollapsingHeader("Material Component", &isOpen, ImGuiTreeNodeFlags_DefaultOpen)) {
                if (isOpen) drawMaterialComponent(scene.get<ecs::MaterialComponent>(entity));
                else scene.remove<ecs::MaterialComponent>(entity);
            }
        }

        if (scene.has<ecs::PointLightComponent>(entity)) {
            bool isOpen = true; // for checking if the close button was clicked
            if (ImGui::CollapsingHeader("Point Light Component", &isOpen, ImGuiTreeNodeFlags_DefaultOpen)) {
                if (isOpen) drawPointLightComponent(scene.get<ecs::PointLightComponent>(entity));
                else scene.remove<ecs::PointLightComponent>(entity);
            }
        }

        if (scene.has<ecs::DirectionalLightComponent>(entity)) {
            bool isOpen = true; // for checking if the close button was clicked
            if (ImGui::CollapsingHeader("Directional Light Component", &isOpen, ImGuiTreeNodeFlags_DefaultOpen)) {
                if (isOpen) drawDirectionalLightComponent(scene.get<ecs::DirectionalLightComponent>(entity));
                else scene.remove<ecs::DirectionalLightComponent>(entity);
            }
        }

        if (scene.has<ecs::MeshAnimationComponent>(entity)) {
            bool isOpen = true;
            if (ImGui::CollapsingHeader("Animation Component", &isOpen, ImGuiTreeNodeFlags_DefaultOpen)) {
                if (isOpen) drawAnimationComponent(scene.get<ecs::MeshAnimationComponent>(entity));
                else scene.remove<ecs::MeshAnimationComponent>(entity);
            }
        }

        if (ImGui::BeginPopup("Components"))
        {
            if (!scene.has<ecs::TransformComponent>(entity)) {
                if (ImGui::Selectable("Transform", false)) {
                    scene.emplace<ecs::TransformComponent>(entity);
                    ImGui::CloseCurrentPopup();
                }
            }

            if (!scene.has<ecs::MeshComponent>(entity)){
                if(ImGui::Selectable("Mesh", false)) {
                    scene.emplace<ecs::MeshComponent>(entity);
                    ImGui::CloseCurrentPopup();
                }
            }

            if (!scene.has<ecs::MaterialComponent>(entity)) {
                if (ImGui::Selectable("Material", false)) {
                    scene.emplace<ecs::MaterialComponent>(entity);
                    ImGui::CloseCurrentPopup();
                }
            }

            if (!scene.has<ecs::PointLightComponent>(entity)) {
                if (ImGui::Selectable("Point Light", false)) {
                    scene.emplace<ecs::PointLightComponent>(entity);
                    ImGui::CloseCurrentPopup();
                }
            }

            if (!scene.has<ecs::DirectionalLightComponent>(entity)) {
                if (ImGui::Selectable("Directional Light", false)) {
                    scene.emplace<ecs::DirectionalLightComponent>(entity);
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndPopup();
        }

        if (ImGui::Button("Add Component", ImVec2(ImGui::GetWindowWidth(), 0))) {
            ImGui::OpenPopup("Components");
        }
    }

    ImGui::End();
}

void InspectorWindow::drawNameComponent(ecs::NameComponent& component) {
    if (ImGui::InputText("Name", &component.name, ImGuiInputTextFlags_AutoSelectAll)) {
        if (component.name.size() > 16) {
            component.name = component.name.substr(0, 16);
        }
    }
}

void InspectorWindow::drawNodeComponent(ecs::NodeComponent& component) {
    if (component.parent == entt::null) {
        ImGui::Text("Parent entity: None");
    } else {
        ImGui::Text("Parent entity: %i", component.parent);

    }
    ImGui::SameLine();
    ImGui::Text("| Has children: %s", component.hasChildren ? "True" : "False");
}

void InspectorWindow::drawTransformComponent(ecs::TransformComponent& component) {
    if (ImGui::DragFloat3("Scale", glm::value_ptr(component.scale))) {
        component.recalculateMatrix();
    }
    if (ImGui::DragFloat3("Rotation", glm::value_ptr(component.rotation), 0.001f, static_cast<float>(-M_PI), static_cast<float>(M_PI))) {
        std::cout << glm::to_string(component.rotation) << '\n';
        component.recalculateMatrix();
    }
    if (ImGui::DragFloat3("Position", glm::value_ptr(component.position), 0.001f, FLT_MIN, FLT_MAX)) {
        component.recalculateMatrix();
    }
}

void InspectorWindow::drawMeshComponent(ecs::MeshComponent& component, entt::registry& scene) {
    ImGui::Text("Vertex count: %i", component.vertices.size());
    ImGui::Text("Index count: %i", component.indices.size() * 3);
    if (scene.valid(component.material) && scene.has<ecs::MaterialComponent, ecs::NameComponent>(component.material)) {
        auto& [material, name] = scene.get<ecs::MaterialComponent, ecs::NameComponent>(component.material);
        ImGui::Image(material.albedo->ImGuiID(), ImVec2(15 * ImGui::GetWindowDpiScale(), 15 * ImGui::GetWindowDpiScale()));
        ImGui::SameLine();
        ImGui::Text(name.name.c_str());

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_mesh_material")) {
                component.material = *reinterpret_cast<const entt::entity*>(payload->Data);
            }
            ImGui::EndDragDropTarget();
        }
    }
}

void InspectorWindow::drawMaterialComponent(ecs::MaterialComponent& component) {
    if (ImGui::ColorEdit4("Base colour", glm::value_ptr(component.baseColour), ImGuiColorEditFlags_Float)) {
        if (component.albedo && component.albedoFile.empty()) {
            glTextureSubImage2D(component.albedo->mID, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, glm::value_ptr(component.baseColour));
        }
    }

    const bool adjustedMetallic = ImGui::DragFloat("Metallic", &component.metallic, 0.001f, 0.0f, 1.0f);
    const bool adjustedRoughness = ImGui::DragFloat("Roughness", &component.roughness, 0.001f, 0.0f, 1.0f);

    if (adjustedMetallic || adjustedRoughness) {
        if (component.metalrough && component.mrFile.empty()) {
            auto metalRoughnessValue = glm::vec4(component.metallic, component.roughness, 0.0f, 1.0f);
            glTextureSubImage2D(component.metalrough->mID, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, glm::value_ptr(metalRoughnessValue));
        }
    }

    if (component.albedo) {
        ImGui::Image(component.albedo->ImGuiID(), ImVec2(15, 15));
        ImGui::SameLine();
    }

    constexpr const char* fileFilters = "Image Files(*.jpg, *.jpeg, *.png)\0*.jpg;*.jpeg;*.png\0";
    
    ImGui::Text("Albedo");
    ImGui::SameLine();
    if (ImGui::SmallButton("... ## albedo")) {
        std::string filepath = OS::openFileDialog(fileFilters);
        if (!filepath.empty()) {
            Stb::Image image;
            image.load(filepath, true);

            if (!component.albedo) {
                component.albedo.reset(new glTexture2D());
            }

            component.albedo->bind();
            component.albedo->init(image.w, image.h, Format::SRGBA_U8, image.pixels);
            component.albedo->setFilter(Sampling::Filter::Trilinear);
            component.albedo->genMipMaps();
            component.albedo->unbind();
        }
    }

    if (component.normals) {
        ImGui::Image(component.normals->ImGuiID(), ImVec2(15, 15));
        ImGui::SameLine();
    }
    
    ImGui::Text("Normal map");
    ImGui::SameLine();
    if (ImGui::SmallButton("... ## normalmap")) {
        std::string filepath = OS::openFileDialog(fileFilters);
        if (!filepath.empty()) {
            Stb::Image image;
            image.load(filepath, true);

            if (!component.normals) {
                component.normals.reset(new glTexture2D());
            }

            component.normals->bind();
            component.normals->init(image.w, image.h, Format::RGBA_U8, image.pixels);
            component.normals->setFilter(Sampling::Filter::None);
            component.normals->unbind();
        }
    }

    if (component.metalrough) {
        ImGui::Image(component.metalrough->ImGuiID(), ImVec2(15, 15));
        ImGui::SameLine();
    }

    ImGui::Text("Metallic Roughness map");
    ImGui::SameLine();
    if (ImGui::SmallButton("... ## metallic roughness")) {
        std::string filepath = OS::openFileDialog(fileFilters);
    }

}

void InspectorWindow::drawPointLightComponent(ecs::PointLightComponent& component) {
    ImGui::ColorEdit4("Colour", glm::value_ptr(component.buffer.colour));
}

void InspectorWindow::drawDirectionalLightComponent(ecs::DirectionalLightComponent& component) {
    ImGui::ColorEdit4("Colour", glm::value_ptr(component.buffer.colour));
    ImGui::DragFloat3("Direction", glm::value_ptr(component.buffer.direction), 0.01f, -1.0f, 1.0f);
}

void InspectorWindow::drawAnimationComponent(ecs::MeshAnimationComponent& component) {
    static bool playing = false;
    ImGui::SliderFloat("Time", &component.animation.runningTime, 0, component.animation.totalDuration);
    if (ImGui::Button(playing ? "pause" : "play")) {
        playing = !playing;
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

bool EntityWindow::drawFamilyNode(entt::registry& scene, entt::entity entity, entt::entity& active) {
    auto selected = active == entity ? ImGuiTreeNodeFlags_Selected : 0;
    auto treeNodeFlags = selected | ImGuiTreeNodeFlags_OpenOnArrow;
    auto name = scene.get<ecs::NameComponent>(entity);
    bool opened = ImGui::TreeNodeEx(name.name.c_str(), treeNodeFlags);
    if (ImGui::IsItemClicked()) {
        active = active == entity ? entt::null : entity;
    }
     return opened;
}

void EntityWindow::drawChildlessNode(entt::registry& scene, entt::entity entity, entt::entity& active) {
    auto name = scene.get<ecs::NameComponent>(entity);
    if (ImGui::Selectable(std::string(name.name + "##" + std::to_string(static_cast<uint32_t>(entity))).c_str(), entity == active)) {
        active = active == entity ? entt::null : entity;
    }
}

void EntityWindow::drawFamily(entt::registry& scene, entt::entity parent, entt::entity& active) {
    auto nodeView = scene.view<ecs::NodeComponent>();
    for (auto entity : nodeView) {
        auto& node = scene.get<ecs::NodeComponent>(entity);
        if(node.parent == parent) {
            if (node.hasChildren) {
                if (drawFamilyNode(scene, entity, active)) {
                    drawFamily(scene, entity, active);
                    ImGui::TreePop();
                }
            } else {
                drawChildlessNode(scene, entity, active);
            }
        }
    }
}

void EntityWindow::draw(entt::registry& scene, entt::entity& active) {
    ImGui::Begin("Scene");

    auto nodeView = scene.view<ecs::NodeComponent>();
    for (auto entity : nodeView) {
        auto& node = nodeView.get<ecs::NodeComponent>(entity);
        if (node.parent == entt::null) {
            if (node.hasChildren) {
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

void Guizmo::drawGuizmo(entt::registry& scene, Viewport& viewport, entt::entity active) {
    if (!scene.valid(active) || !enabled || !scene.has<ecs::TransformComponent>(active)) return;
    auto& transform = scene.get<ecs::TransformComponent>(active);

    // set the gizmo's viewport
    ImGuizmo::SetDrawlist();
    auto pos = ImGui::GetWindowPos();
    ImGuizmo::SetRect(pos.x, pos.y, (float)viewport.size.x, (float)viewport.size.y);

    // temporarily transform to mesh space for gizmo use
    auto mesh = scene.try_get<ecs::MeshComponent>(active);
    if (mesh) transform.matrix = glm::translate(transform.matrix, ((mesh->aabb[0] + mesh->aabb[1]) / 2.0f));

    // draw gizmo
    ImGuizmo::Manipulate(
        glm::value_ptr(viewport.getCamera().getView()),
        glm::value_ptr(viewport.getCamera().getProjection()),
        operation, ImGuizmo::MODE::LOCAL,
        glm::value_ptr(transform.matrix)
    );

    // transform back to world space
    if (mesh) transform.matrix = glm::translate(transform.matrix, -((mesh->aabb[0] + mesh->aabb[1]) / 2.0f));


    // update the transformation
    ImGuizmo::DecomposeMatrixToComponents(
        glm::value_ptr(transform.matrix),
        glm::value_ptr(transform.position),
        glm::value_ptr(transform.rotation),
        glm::value_ptr(transform.scale)
    );

    transform.rotation = glm::radians(transform.rotation);
}

void Guizmo::drawWindow() {
    ImGui::Begin("Editor");
    if (ImGui::Checkbox("Gizmo", &enabled)) {
        ImGuizmo::Enable(enabled);
    }

    ImGui::SameLine();

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