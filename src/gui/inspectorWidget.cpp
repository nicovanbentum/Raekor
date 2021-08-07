#include "pch.h"
#include "inspectorWidget.h"
#include "editor.h"
#include "../platform/OS.h"

namespace Raekor {

InspectorWidget::InspectorWidget(Editor* editor) : IWidget(editor, "Inspector") {}



void InspectorWidget::draw() {
    ImGui::Begin(title.c_str());

    if (editor->active == entt::null) {
        ImGui::End();
        return;
    }

    ImGui::Text("ID: %i", editor->active);

    Scene& scene = IWidget::scene();
    Assets& assets = IWidget::assets();
    entt::entity& active = editor->active;

    // I much prefered the for_each_tuple_element syntax tbh
    std::apply([&](const auto& ... components) {
        (..., [&](Assets& assets, Scene& scene, entt::entity& entity) {
            using ComponentType = typename std::decay<decltype(components)>::type::type;

            if (scene.has<ComponentType>(entity)) {
                bool isOpen = true;
                if (ImGui::CollapsingHeader(components.name, &isOpen, ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (isOpen) {
                        drawComponent(scene.get<ComponentType>(entity), assets, scene, entity);
                    } else {
                        scene.remove<ComponentType>(entity);
                    }
                }
            }
        }(assets, scene, active));
    }, Components);

    if (ImGui::BeginPopup("Components")) {
        if (ImGui::Selectable("Native Script", false)) {
            scene.emplace<NativeScript>(active);
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



void InspectorWidget::drawComponent(Name& component, Assets& assets, Scene& scene, entt::entity& active) {
    ImGui::InputText("Name##1", &component.name, ImGuiInputTextFlags_AutoSelectAll);
}



void InspectorWidget::drawComponent(Node& component, Assets& assets, Scene& scene, entt::entity& active) {
    ImGui::Text("Parent entity: %i", component.parent);
    ImGui::Text("Siblings: %i, %i", component.prevSibling, component.nextSibling);
}



void InspectorWidget::drawComponent(Mesh& component, Assets& assets, Scene& scene, entt::entity& active) {
    ImGui::Text("Triangle count: %i", component.indices.size() / 3);

    if (scene.valid(component.material) && scene.has<Material, Name>(component.material)) {
        auto& [material, name] = scene.get<Material, Name>(component.material);

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



void InspectorWidget::drawComponent(Skeleton& component, Assets& assets, Scene& scene, entt::entity& active) {
    static bool playing = false;
    ImGui::SliderFloat("Time", &component.animation.runningTime, 0, component.animation.totalDuration);
    if (ImGui::Button(playing ? "pause" : "play")) {
        playing = !playing;
    }
}



void InspectorWidget::drawComponent(Material& component, Assets& assets, Scene& scene, entt::entity& active) {
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

    const char* fileFilters = "Image Files(*.jpg, *.jpeg, *.png)\0*.jpg;*.jpeg;*.png\0";

    using createTextureFnType = void(Material::*)(std::shared_ptr<TextureAsset> texture);

    auto drawTextureInteraction = [=](GLuint texture,const char* name, Material* component, createTextureFnType create) {
        ImGui::PushID(texture);

        bool usingTexture = texture != 0;
        if (ImGui::Checkbox("", &usingTexture)) {
        }

        ImGui::PopID();

        ImGui::SameLine();

        const GLuint image = texture ? texture : Material::Default.albedo;

        if (ImGui::ImageButton((void*)((intptr_t)image), ImVec2(lineHeight - 1, lineHeight - 1))) {
            auto filepath = OS::openFileDialog(fileFilters);
            if (!filepath.empty()) {
                auto assetPath = TextureAsset::convert(filepath);
                (component->*create)(IWidget::assets().get<TextureAsset>(assetPath));
            }
        }

        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::Text(name);
    };

    drawTextureInteraction(component.albedo, component.albedoFile.c_str(), &component, &Material::createAlbedoTexture);
    drawTextureInteraction(component.normals, component.normalFile.c_str(), &component, &Material::createNormalTexture);
    drawTextureInteraction(component.metalrough, component.mrFile.c_str(), &component, &Material::createMetalRoughTexture);
}



void InspectorWidget::drawComponent(Transform& component, Assets& assets, Scene& scene, entt::entity& active) {
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



void InspectorWidget::drawComponent(PointLight& component, Assets& assets, Scene& scene, entt::entity& active) {
    ImGui::ColorEdit4("Colour", glm::value_ptr(component.colour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
}



void InspectorWidget::drawComponent(NativeScript& component, Assets& assets, Scene& scene, entt::entity& active) {
    if (component.asset) {
        ImGui::Text("Module: %i", component.asset->getModule());
        ImGui::Text("File: %s", component.file.c_str());
    }

    if (ImGui::Button("Load..")) {
        std::string filepath = OS::openFileDialog("DLL Files (*.dll)\0*.dll\0");
        if (!filepath.empty()) {
            component.file = fs::relative(filepath).string();
            component.asset = assets.get<ScriptAsset>(component.file);
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Release")) {
        assets.release(component.file);
        delete component.script;
        component = {};
    }

    ImGui::SameLine();

    if (ImGui::Button("Recompile")) {
        std::string filepath = OS::openFileDialog("C++ Files (*.cpp)\0*.cpp\0");

        if (!filepath.empty()) {
            std::string procAddress = component.procAddress;
            
            delete component.script;
            assets.release(component.file);
            component = NativeScript();
            
            std::string asset =  ScriptAsset::convert(filepath);
            
            if (!asset.empty()) {
                component.file = asset;
                component.procAddress = procAddress;
                component.asset = assets.get<ScriptAsset>(asset);

                scene.bindScript(active, component);
            }
        }
    }

    if (ImGui::InputText("Function", &component.procAddress, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (component.asset) {
            scene.bindScript(active, component);
        }
    }
}



void InspectorWidget::drawComponent(DirectionalLight& component, Assets& assets, Scene& scene, entt::entity& active) {
    ImGui::ColorEdit4("Colour", glm::value_ptr(component.colour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    ImGui::DragFloat3("Direction", glm::value_ptr(component.direction), 0.01f, -1.0f, 1.0f);
}

}