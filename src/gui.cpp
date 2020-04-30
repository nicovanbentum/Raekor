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
            if (ImGui::CollapsingHeader("Transform Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                drawTransformComponent(scene.transforms.getComponent(entity));
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

}

void InspectorWindow::drawMeshRendererComponent(ECS::MeshRendererComponent* component) {

}

void InspectorWindow::drawMaterialComponent(ECS::MaterialComponent* component) {
    
}

} // gui
} // raekor