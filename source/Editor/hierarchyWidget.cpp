#include "pch.h"
#include "editor.h"
#include "hierarchyWidget.h"
#include "Raekor/systems.h"
#include "Raekor/input.h"

namespace Raekor {

HierarchyWidget::HierarchyWidget(Editor* editor) : IWidget(editor, "Scene") {}



void HierarchyWidget::draw(float dt) {
    ImGui::Begin(title.c_str());

    auto& scene = IWidget::scene();
    auto nodes = scene.view<Node>();
    
    for (auto& [entity, node] : nodes.each()) {
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

    dropTargetWindow(scene);

    ImGui::End();
}



bool HierarchyWidget::drawFamilyNode(entt::registry& scene, entt::entity entity, entt::entity& active) {
    auto name = scene.get<Name>(entity);
    auto selected = active == entity ? ImGuiTreeNodeFlags_Selected : 0;
    auto treeNodeFlags = selected | ImGuiTreeNodeFlags_OpenOnArrow;
    
    bool opened = ImGui::TreeNodeEx(name.name.c_str(), treeNodeFlags);
    
    if (ImGui::IsItemClicked()) {
        active = active == entity ? entt::null : entity;
    }

    dropTargetNode(scene, entity);

    return opened;
}



void HierarchyWidget::drawChildlessNode(entt::registry& scene, entt::entity entity, entt::entity& active) {
    auto name = scene.get<Name>(entity);
    auto selectableName = name.name + "##" + std::to_string(entt::to_integral(entity));

    if (ImGui::Selectable(selectableName.c_str(), entity == active)) {
        active = active == entity ? entt::null : entity;
    }

    dropTargetNode(scene, entity);
}



void HierarchyWidget::dropTargetNode(entt::registry& scene, entt::entity entity) {

    ImGuiDragDropFlags src_flags = ImGuiDragDropFlags_SourceNoDisableHover;
    src_flags |= ImGuiDragDropFlags_SourceNoHoldToOpenOthers;
    if (ImGui::BeginDragDropSource(src_flags)) {
        ImGui::SetDragDropPayload("drag_drop_hierarchy_entity", &entity, sizeof(entt::entity));
        ImGui::EndDragDropSource();
    }
    
    auto& node = scene.get<Node>(entity);

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_hierarchy_entity")) {
            entt::entity child = *reinterpret_cast<const entt::entity*>(payload->Data);

            bool childIsParent = false;

            for (auto parent = node.parent; parent != entt::null; parent = scene.get<Node>(parent).parent) {
                if (child == parent) {
                    childIsParent = true;
                    break;
                }
            }

            if (!childIsParent) {
                auto& childTransform = scene.get<Transform>(entity);
                auto& parentTransform = scene.get<Transform>(entity);


                NodeSystem::remove(scene, scene.get<Node>(child));
                NodeSystem::append(scene, node, scene.get<Node>(child));
            }

        }

        ImGui::EndDragDropTarget();
    }
}



void HierarchyWidget::dropTargetWindow(entt::registry& scene) {
    if (ImGui::BeginDragDropTargetCustom(ImGui::GetCurrentWindow()->InnerRect, ImGui::GetCurrentWindow()->ID)) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_hierarchy_entity")) {
            entt::entity entity = *reinterpret_cast<const entt::entity*>(payload->Data);
            auto& node = scene.get<Node>(entity);
            auto& transform = scene.get<Transform>(entity);

            transform.localTransform = transform.worldTransform;
            transform.decompose();

            NodeSystem::remove(scene, node);
            node.parent = entt::null;

        }

        ImGui::EndDragDropTarget();
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