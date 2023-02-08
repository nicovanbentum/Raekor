#include "pch.h"
#include "editor.h"
#include "hierarchyWidget.h"
#include "Raekor/systems.h"
#include "Raekor/input.h"

namespace Raekor {

RTTI_CLASS_CPP_NO_FACTORY(HierarchyWidget) {}


HierarchyWidget::HierarchyWidget(Editor* editor) : IWidget(editor, ICON_FA_STREAM " Scene ") {}


void HierarchyWidget::draw(float dt) {
    ImGui::Begin(m_Title.c_str(), &m_Visible);

    auto& scene = GetScene();
    auto& active_entity = GetActiveEntity();
    auto nodes = scene.view<Node>();
    
    for (auto& [entity, node] : nodes.each()) {
        if (!node.IsRoot())
            continue;

        if (node.HasChildren()) {
            if (drawFamilyNode(scene, entity, active_entity)) {
                drawFamily(scene, entity, active_entity);
                ImGui::TreePop();
            }
        } 
        else 
            drawChildlessNode(scene, entity, active_entity);
    }

    dropTargetWindow(scene);

    ImGui::End();
}


bool HierarchyWidget::drawFamilyNode(Scene& scene, Entity entity, Entity& active) {
    const auto name = scene.get<Name>(entity);
    const auto selected = active == entity ? ImGuiTreeNodeFlags_Selected : 0;
    const auto tree_flags = selected | ImGuiTreeNodeFlags_OpenOnArrow;
    
    bool opened = ImGui::TreeNodeEx(std::string(ICON_FA_CUBE "   " + name.name).c_str(), tree_flags);
    
    if (ImGui::IsItemClicked())
        active = active == entity ? entt::null : entity;

    dropTargetNode(scene, entity);

    return opened;
}


void HierarchyWidget::drawChildlessNode(Scene& scene, Entity entity, Entity& active) {
    auto name = scene.get<Name>(entity);
    auto selectable_name = name.name + "##" + std::to_string(entt::to_integral(entity));

    if (ImGui::Selectable(std::string(ICON_FA_CUBE "   " + selectable_name).c_str(), entity == active))
        active = active == entity ? entt::null : entity;

    dropTargetNode(scene, entity);
}


void HierarchyWidget::dropTargetNode(Scene& scene, Entity entity) {
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

                NodeSystem::sRemove(scene, scene.get<Node>(child));
                NodeSystem::sAppend(scene, node, scene.get<Node>(child));
            }

        }

        ImGui::EndDragDropTarget();
    }
}


void HierarchyWidget::dropTargetWindow(Scene& scene) {
    if (ImGui::BeginDragDropTargetCustom(ImGui::GetCurrentWindow()->InnerRect, ImGui::GetCurrentWindow()->ID)) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_hierarchy_entity")) {
            entt::entity entity = *reinterpret_cast<const entt::entity*>(payload->Data);
            auto& node = scene.get<Node>(entity);
            auto& transform = scene.get<Transform>(entity);

            transform.localTransform = transform.worldTransform;
            transform.Decompose();

            NodeSystem::sRemove(scene, node);
            node.parent = entt::null;

        }

        ImGui::EndDragDropTarget();
    }
}


void HierarchyWidget::drawFamily(Scene& scene, Entity entity, Entity& active) {
    const auto& node = scene.get<Node>(entity);

    if (node.HasChildren()) {
        for (auto it = node.firstChild; it != entt::null; it = scene.get<Node>(it).nextSibling) {
            const auto& child = scene.get<Node>(it);

            if (child.HasChildren()) {
                if (drawFamilyNode(scene, it, active)) {
                    drawFamily(scene, it, active);
                    ImGui::TreePop();
                }
            } 
            else
                drawChildlessNode(scene, it, active);
        }
    }
}

} // raekor