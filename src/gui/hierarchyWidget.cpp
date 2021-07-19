#include "pch.h"
#include "editor.h"
#include "hierarchyWidget.h"

namespace Raekor {

HierarchyWidget::HierarchyWidget(Editor* editor) : IWidget(editor, "Scene") {}



void HierarchyWidget::draw() {
    ImGui::Begin(title.c_str());

    auto& scene = IWidget::scene();

    auto nodeView = scene.view<Node>();
    for (auto entity : nodeView) {
        auto& node = nodeView.get<Node>(entity);

        if (node.parent == entt::null) {
            if (node.firstChild != entt::null) {
                if (drawFamilyNode(scene, entity, editor->active)) {
                    drawFamily(scene, entity, editor->active);
                    ImGui::TreePop();
                }
            } else {
                drawChildlessNode(scene, entity, editor->active);
            }
        }
    }

    ImGui::End();
}



bool HierarchyWidget::drawFamilyNode(entt::registry& scene, entt::entity entity, entt::entity& active) {
    auto& node = scene.get<Node>(entity);

    auto selected = active == entity ? ImGuiTreeNodeFlags_Selected : 0;
    auto treeNodeFlags = selected | ImGuiTreeNodeFlags_OpenOnArrow;
    auto name = scene.get<Name>(entity);
    bool opened = ImGui::TreeNodeEx(name.name.c_str(), treeNodeFlags);
    if (ImGui::IsItemClicked()) {
        active = active == entity ? entt::null : entity;
    }
    return opened;
}



void HierarchyWidget::drawChildlessNode(entt::registry& scene, entt::entity entity, entt::entity& active) {
    auto& node = scene.get<Node>(entity);
    auto name = scene.get<Name>(entity);
    if (ImGui::Selectable(std::string(name.name + "##" + std::to_string(static_cast<uint32_t>(entity))).c_str(), entity == active)) {
        active = active == entity ? entt::null : entity;
    }
}



void HierarchyWidget::drawFamily(entt::registry& scene, entt::entity entity, entt::entity& active) {
    auto& node = scene.get<Node>(entity);

    if (node.firstChild != entt::null) {
        for (auto it = node.firstChild; it != entt::null; it = scene.get<Node>(it).nextSibling) {
            auto& itNode = scene.get<Node>(it);

            if (itNode.firstChild != entt::null) {
                if (drawFamilyNode(scene, it, active)) {
                    drawFamily(scene, it, active);
                    ImGui::TreePop();
                }
            } else {
                drawChildlessNode(scene, it, active);
            }
        }
    }
}

} // raekor