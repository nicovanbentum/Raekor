#include "pch.h"
#include "inspectorWidget.h"
#include "editor.h"
#include "../platform/OS.h"

namespace Raekor {

InspectorWidget::InspectorWidget(Editor* editor) : IWidget(editor) {}



void InspectorWidget::drawComponent(ecs::NameComponent& component, entt::registry& scene, entt::entity& active) {
    ImGui::InputText("Name##1", &component.name, ImGuiInputTextFlags_AutoSelectAll);
}



void InspectorWidget::drawComponent(ecs::NodeComponent& component, entt::registry& scene, entt::entity& active) {
    ImGui::Text("Parent entity: %i", component.parent);
    ImGui::Text("Siblings: %i, %i", component.prevSibling, component.nextSibling);
}



void InspectorWidget::drawComponent(ecs::TransformComponent& component, entt::registry& scene, entt::entity& active) {
    if (ImGui::DragFloat3("Scale", glm::value_ptr(component.scale), 0.001f, 0.0f, FLT_MAX)) {
        component.compose();
    }
    if (ImGui::DragFloat3("Rotation", glm::value_ptr(component.rotation), 0.001f, static_cast<float>(-M_PI), static_cast<float>(M_PI))) {
        component.compose();
    }
    if (ImGui::DragFloat3("Position", glm::value_ptr(component.position), 0.001f, FLT_MIN, FLT_MAX)) {
        component.compose();
    }
}



void InspectorWidget::drawComponent(ecs::MeshComponent& component, entt::registry& scene, entt::entity& active) {
    ImGui::Text("Triangle count: %i", component.indices.size() / 3);

    if (scene.valid(component.material) && scene.has<ecs::MaterialComponent, ecs::NameComponent>(component.material)) {
        auto& [material, name] = scene.get<ecs::MaterialComponent, ecs::NameComponent>(component.material);

        const auto albedoTexture = (void*)((intptr_t)material.albedo);
        const auto previewSize = ImVec2(10 * ImGui::GetWindowDpiScale(), 10 * ImGui::GetWindowDpiScale());
        const auto tintColor = ImVec4(material.baseColour.r, material.baseColour.g, material.baseColour.b, material.baseColour.a);

        if (ImGui::ImageButton(albedoTexture, previewSize, ImVec2(0, 0), ImVec2(1, 1), -1, ImVec4(0, 0, 0, 0), tintColor)) {
            active = component.material;
        }

        ImGui::SameLine();
        ImGui::Text(name.name.c_str());
    } else {
        ImGui::Text("No material");
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_mesh_material")) {
            component.material = *reinterpret_cast<const entt::entity*>(payload->Data);
        }
        ImGui::EndDragDropTarget();
    }
}



void InspectorWidget::drawComponent(ecs::MaterialComponent& component, entt::registry& scene, entt::entity& active) {
    auto& io = ImGui::GetIO();
    auto& style = ImGui::GetStyle();
    float lineHeight = io.FontDefault->FontSize;

    ImGui::ColorEdit4("Base colour", glm::value_ptr(component.baseColour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);

    const bool adjustedMetallic = ImGui::DragFloat("Metallic", &component.metallic, 0.001f, 0.0f, 1.0f);
    const bool adjustedRoughness = ImGui::DragFloat("Roughness", &component.roughness, 0.001f, 0.0f, 1.0f);

    if (adjustedMetallic || adjustedRoughness) {
        if (component.mrFile.empty() && component.metalrough) {
            auto metalRoughnessValue = glm::vec4(0.0f, component.roughness, component.metallic, 1.0f);
            glTextureSubImage2D(component.metalrough, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, glm::value_ptr(metalRoughnessValue));
        }
    }

    constexpr char* fileFilters = "Image Files(*.jpg, *.jpeg, *.png)\0*.jpg;*.jpeg;*.png\0";

    auto drawTextureInteraction = [this, fileFilters, lineHeight](
        GLuint texture,
        const char* name,
        ecs::MaterialComponent* component,
        void(ecs::MaterialComponent::* func)(std::shared_ptr<TextureAsset> texture)) {
        ImGui::PushID(texture);

        bool usingTexture = texture != 0;
        if (ImGui::Checkbox("", &usingTexture)) {
        }

        ImGui::PopID();

        ImGui::SameLine();

        const GLuint image = texture ? texture : ecs::MaterialComponent::Default.albedo;

        if (ImGui::ImageButton((void*)((intptr_t)image), ImVec2(lineHeight - 1, lineHeight - 1))) {
            auto filepath = OS::openFileDialog(fileFilters);
            if (!filepath.empty()) {
                auto assetPath = TextureAsset::create(filepath);
                (component->*func)(editor->assetManager.get<TextureAsset>(assetPath));
            }
        }

        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::Text(name);
    };

    drawTextureInteraction(component.albedo, component.albedoFile.c_str(), &component, &ecs::MaterialComponent::createAlbedoTexture);
    drawTextureInteraction(component.normals, component.normalFile.c_str(), &component, &ecs::MaterialComponent::createNormalTexture);
    drawTextureInteraction(component.metalrough, component.mrFile.c_str(), &component, &ecs::MaterialComponent::createMetalRoughTexture);
}



void InspectorWidget::drawComponent(ecs::PointLightComponent& component, entt::registry& scene, entt::entity& active) {
    ImGui::ColorEdit4("Colour", glm::value_ptr(component.buffer.colour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
}



void InspectorWidget::drawComponent(ecs::DirectionalLightComponent& component, entt::registry& scene, entt::entity& active) {
    ImGui::ColorEdit4("Colour", glm::value_ptr(component.buffer.colour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    ImGui::DragFloat3("Direction", glm::value_ptr(component.buffer.direction), 0.01f, -1.0f, 1.0f);
}



void InspectorWidget::drawComponent(ecs::MeshAnimationComponent& component, entt::registry& scene, entt::entity& active) {
    static bool playing = false;
    ImGui::SliderFloat("Time", &component.animation.runningTime, 0, component.animation.totalDuration);
    if (ImGui::Button(playing ? "pause" : "play")) {
        playing = !playing;
    }
}



void InspectorWidget::drawComponent(ecs::NativeScriptComponent& component, entt::registry& scene, entt::entity& active) {
    if (!component.hmodule) {
        if (ImGui::Button("Load DLL..")) {
            std::string filepath = OS::openFileDialog("DLL Files (*.dll)\0*.dll\0");
            component.hmodule = LoadLibraryA(filepath.c_str());
        }
    } else {
        ImGui::Text("Module: %i", component.hmodule);
    }
    if (ImGui::InputText("Function", &component.procAddress, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (component.hmodule) {
            auto address = GetProcAddress(component.hmodule, component.procAddress.c_str());
            if (address) {
                auto function = reinterpret_cast<NativeScript::FactoryType>(address);
                component.script = function();
                component.script->bind(active, scene);
                std::puts("newing from proc address!");
            }
        }
    }
}



void InspectorWidget::draw() {
    ImGui::Begin("Inspector");

    if (editor->active == entt::null) {
        ImGui::End();
        return;
    }

    ImGui::Text("ID: %i", editor->active);

    entt::registry& scene = editor->scene;
    entt::entity& active = editor->active;

    // I much prefered the for_each_tuple_element syntax tbh
    std::apply([this, &scene, &active](const auto & ... components) {
        (...,
            [&components, this](entt::registry& scene, entt::entity& entity) {
            using ComponentType = typename std::decay<decltype(components)>::type::type;

            if (scene.has<ComponentType>(entity)) {
                bool isOpen = true;
                if (ImGui::CollapsingHeader(components.name, ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (isOpen) {
                        drawComponent(scene.get<ComponentType>(entity), scene, entity);
                    } else {
                        scene.remove<ComponentType>(entity);
                    }
                }
            }
        }(scene, active));
    }, ecs::Components);

    if (ImGui::BeginPopup("Components")) {
        if (ImGui::Selectable("Native Script", false)) {
            scene.emplace<ecs::NativeScriptComponent>(active);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Broken for now
    if (ImGui::Button("Add Component", ImVec2(ImGui::GetWindowWidth(), 0))) {
        ImGui::OpenPopup("Components");
    }

    ImGui::End();
};

}