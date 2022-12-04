#include "pch.h"
#include "inspectorWidget.h"
#include "editor.h"
#include "Raekor/OS.h"

namespace Raekor {

InspectorWidget::InspectorWidget(Editor* editor) : IWidget(editor, "Inspector") {}


void InspectorWidget::draw(float dt) {
    ImGui::Begin(title.c_str());

    auto& active_entity = GetActiveEntity();

    if (active_entity == entt::null) {
        ImGui::End();
        return;
    }

    ImGui::Text("Entity ID: %i", active_entity);

    Scene& scene = GetScene();
    Assets& assets = GetAssets();
    entt::entity& active = active_entity;

    // I much prefered the for_each_tuple_element syntax tbh
    std::apply([&](const auto& ... components) {
        (..., [&](Assets& assets, Scene& scene, entt::entity& entity) {
            using ComponentType = typename std::decay<decltype(components)>::type::type;

            if (scene.all_of<ComponentType>(entity)) {
                bool isOpen = true;
                if (ImGui::CollapsingHeader(components.name, &isOpen, ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (isOpen)
                        DrawComponent(scene.get<ComponentType>(entity), entity);
                    else
                        scene.remove<ComponentType>(entity);
                }
            }
        }(assets, scene, active));
    }, Components);

    if (ImGui::BeginPopup("Components")) {
        if (ImGui::Selectable("Native Script", false)) {
            scene.emplace<NativeScript>(active);
            ImGui::CloseCurrentPopup();
        }

        if (scene.all_of<Transform, Mesh>(active)) {
            if (ImGui::Selectable("Box Collider", false)) {
                auto& collider = scene.emplace<BoxCollider>(active);
                const auto& [transform, mesh] = scene.get<Transform, Mesh>(active);

                const auto half_extent = glm::abs(mesh.aabb[1] - mesh.aabb[0]) / 2.0f * transform.scale;
                collider.settings.mHalfExtent = JPH::Vec3(half_extent.x, half_extent.y, half_extent.z);
                //collider.settings.SetEmbedded();
            }
        }
       
        ImGui::EndPopup();
    }

    // Broken for now
    if (ImGui::Button("Add Component", ImVec2(ImGui::GetWindowWidth(), 0))) {
        ImGui::OpenPopup("Components");
    }

    ImGui::End();
};


void InspectorWidget::DrawComponent(Name& component, Entity& active) {
    ImGui::InputText("Name##1", &component.name, ImGuiInputTextFlags_AutoSelectAll);
}


void InspectorWidget::DrawComponent(Node& component, Entity& active) {
    if (component.parent != sInvalidEntity)
        ImGui::Text("Parent entity: %i", component.parent);
    else
        ImGui::Text("Parent entity: NULL");

    ImGui::Text("Siblings: %i, %i", component.prevSibling, component.nextSibling);
}


void InspectorWidget::DrawComponent(Mesh& component, Entity& active) {
    ImGui::Text("Triangle count: %i", component.indices.size() / 3);
    ImGui::Text("Position count: %i", component.positions.size());
    ImGui::Text("TexCoord count: %i", component.uvs.size());
    ImGui::Text("Normals count: %i", component.normals.size());
    ImGui::Text("Tangents count: %i", component.tangents.size());

    auto& scene = GetScene();
    if (scene.valid(component.material) && scene.all_of<Material, Name>(component.material)) {
        auto& [material, name] = scene.get<Material, Name>(component.material);

        const auto albedoTexture = (void*)((intptr_t)material.gpuAlbedoMap);
        const auto previewSize = ImVec2(10 * ImGui::GetWindowDpiScale(), 10 * ImGui::GetWindowDpiScale());
        const auto tintColor = ImVec4(material.albedo.r, material.albedo.g, material.albedo.b, material.albedo.a);

        if (ImGui::ImageButton(albedoTexture, previewSize))
            active = component.material;

        ImGui::SameLine();
        ImGui::Text(name.name.c_str());
    } else {
        ImGui::Text("No material");
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_mesh_material"))
            component.material = *reinterpret_cast<const entt::entity*>(payload->Data);

        ImGui::EndDragDropTarget();
    }
}


void InspectorWidget::DrawComponent(BoxCollider& component, Entity& active) {
    auto& body_interface = GetPhysics().GetSystem().GetBodyInterface();

    ImGui::Text("Unique Body ID: %i", component.bodyID.GetIndexAndSequenceNumber());

    constexpr std::array items = { "Static", "Kinematic", "Dynamic" };

    int index = int(component.motionType);
    if (ImGui::Combo("##MotionType", &index, items.data(), int(items.size()))) {
        component.motionType = JPH::EMotionType(index);
        body_interface.SetMotionType(component.bodyID, component.motionType, JPH::EActivation::Activate);
    }
}


void InspectorWidget::DrawComponent(Skeleton& component, Entity& active) {
    static bool playing = false;
    const float currentTime = component.animations[0].GetRunningTime();
    const float totalDuration = component.animations[0].GetTotalDuration();

    ImGui::ProgressBar(currentTime / totalDuration);

    if (ImGui::Button(playing ? "pause" : "play"))
        playing = !playing;

    ImGui::SameLine();

    if (ImGui::BeginCombo("Animation##InspectorWidget::drawComponent", component.animations[0].GetName().c_str())) {
        for (auto& animation : component.animations) {
            ImGui::Selectable(animation.GetName().c_str());
        }

        ImGui::EndCombo();
    }

}


void InspectorWidget::DrawComponent(Material& component, Entity& active) {
    auto& io = ImGui::GetIO();
    auto& style = ImGui::GetStyle();
    float lineHeight = io.FontDefault->FontSize;

    ImGui::ColorEdit4("Albedo", glm::value_ptr(component.albedo), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    ImGui::ColorEdit3("Emissive", glm::value_ptr(component.emissive), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);

    const bool adjustedMetallic = ImGui::DragFloat("Metallic", &component.metallic, 0.001f, 0.0f, 1.0f);
    const bool adjustedRoughness = ImGui::DragFloat("Roughness", &component.roughness, 0.001f, 0.0f, 1.0f);

    if (adjustedMetallic || adjustedRoughness) {
        if (component.metalroughFile.empty() && component.gpuMetallicRoughnessMap) {
            auto metalRoughnessValue = glm::vec4(0.0f, component.roughness, component.metallic, 1.0f);
            glTextureSubImage2D(component.gpuMetallicRoughnessMap, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, glm::value_ptr(metalRoughnessValue));
        }
    }

    if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {

        ImGui::Text("Asset conversion failed. See console output log.\n");

        if (ImGui::Button("OK"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    auto drawTextureInteraction = [&](GLuint& gpuMap, std::string& file) {
        const GLuint image = gpuMap ? gpuMap : Material::Default.gpuAlbedoMap;
        
        if (ImGui::ImageButton((void*)((intptr_t)image), ImVec2(lineHeight - 1, lineHeight - 1))) {
            auto filepath = OS::sOpenFileDialog("Image Files(*.jpg, *.jpeg, *.png)\0*.jpg;*.jpeg;*.png\0");

            if (!filepath.empty()) {
                const auto asset_path = TextureAsset::sConvert(filepath);

                if (!asset_path.empty()) {
                    file = asset_path;
                    gpuMap = GLRenderer::sUploadTextureFromAsset(GetAssets().Get<TextureAsset>(asset_path));
                }
                else {
                    ImGui::OpenPopup("Error");
                }
            }
        }

        if (!file.empty()) {
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::Text(file.c_str());

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(file.c_str());
        }
    };

    ImGui::AlignTextToFramePadding(); ImGui::Text("Albedo Map"); ImGui::SameLine();
    drawTextureInteraction(component.gpuAlbedoMap, component.albedoFile);
    ImGui::AlignTextToFramePadding(); ImGui::Text("Normal Map"); ImGui::SameLine();
    drawTextureInteraction(component.gpuNormalMap, component.normalFile);
    ImGui::AlignTextToFramePadding(); ImGui::Text("Material Map"); ImGui::SameLine();
    drawTextureInteraction(component.gpuMetallicRoughnessMap, component.metalroughFile);

    ImGui::Checkbox("Is Transparent", &component.isTransparent);
}


bool dragVec3(const char* label, glm::vec3& v, float step, float min, float max, const char* format = "%.2f") {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    bool value_changed = false;
    ImGui::PushID(label);
    ImGui::PushMultiItemsWidths(v.length(), ImGui::CalcItemWidth());
    
    constexpr std::array chars = { "X", "Y", "Z", "W" };

    const std::array colors = {
        ImVec4{0.5f, 0.0f, 0.0f, 1.0f},
        ImVec4{0.0f, 0.5f, 0.0f, 1.0f},
        ImVec4{0.1f, 0.1f, 1.0f, 1.0f}
    };

    for (int i = 0; i < v.length(); i++) {
        ImGui::PushID(i);

        if (i > 0)
            ImGui::SameLine(0, ImGui::GetStyle().FramePadding.x);
        
        //ImGui::PushStyleColor(ImGuiCol_Button, colors[i]);
        //
        //if (ImGui::Button(chars[i])) {
        //    v[i] = 0.0f;
        //    value_changed = true;
        //}

        //ImGui::PopStyleColor();

        //ImGui::SameLine(0, 1);
        auto size = ImGui::CalcTextSize("12.456");
        ImGui::PushItemWidth(size.x);

        ImGuiDataType type = ImGuiDataType_Float;
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, colors[i]);
        value_changed |= ImGui::DragScalar("", type, (void*)&v[i], step, &min, &max, format, 0);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        /*ImGui::SameLine();  ImGui::Spacing();*/

        ImGui::PopItemWidth();
        
        ImGui::PopItemWidth();
        ImGui::PopID();
    }
    ImGui::PopID();

    const char* label_end = ImGui::FindRenderedTextEnd(label);
    if (label != label_end) {
        ImGui::SameLine(0, g.Style.ItemInnerSpacing.x);
        ImGui::TextEx(label, label_end);
    }

    return value_changed;
}


void InspectorWidget::DrawComponent(Transform& component, Entity& active) {
    if (dragVec3("Scale", component.scale, 0.001f, 0.0f, FLT_MAX))
        component.Compose();

    auto degrees = glm::degrees(glm::eulerAngles(component.rotation));

    if (dragVec3("Rotation", degrees, 0.1f, -360.0f, 360.0f)) {
        component.rotation = glm::quat(glm::radians(degrees));
        component.Compose();
    }

    if (dragVec3("Position", component.position, 0.001f, -FLT_MAX, FLT_MAX))
        component.Compose();
}


void InspectorWidget::DrawComponent(PointLight& component, Entity& active) {
    ImGui::ColorEdit4("Colour", glm::value_ptr(component.colour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
}


void InspectorWidget::DrawComponent(NativeScript& component, Entity& active) {
    auto& scene = GetScene();
    auto& assets = GetAssets();

    if (component.asset) {
        ImGui::Text("Module: %i", component.asset->GetModule());
        ImGui::Text("File: %s", component.file.c_str());
    }

    if (ImGui::Button("Load..")) {
        std::string filepath = OS::sOpenFileDialog("DLL Files (*.dll)\0*.dll\0");
        if (!filepath.empty()) {
            component.file = FileSystem::relative(filepath).string();
            component.asset = assets.Get<ScriptAsset>(component.file);
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Release")) {
        assets.Release(component.file);
        delete component.script;
        component = {};
    }

    ImGui::SameLine();

    if (ImGui::Button("Recompile")) {
        std::string filepath = OS::sOpenFileDialog("C++ Files (*.cpp)\0*.cpp\0");

        if (!filepath.empty()) {
            std::string procAddress = component.procAddress;
            
            delete component.script;
            assets.Release(component.file);
            component = NativeScript();
            
            std::string asset =  ScriptAsset::sConvert(filepath);
            
            if (!asset.empty()) {
                component.file = asset;
                component.procAddress = procAddress;
                component.asset = assets.Get<ScriptAsset>(asset);

                scene.BindScriptToEntity(active, component);
            }
        }
    }

    if (ImGui::InputText("Function", &component.procAddress, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (component.asset) {
            scene.BindScriptToEntity(active, component);
        }
    }
}


void InspectorWidget::DrawComponent(DirectionalLight& component, Entity& active) {
    ImGui::ColorEdit4("Colour", glm::value_ptr(component.colour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    ImGui::DragFloat3("Direction", glm::value_ptr(component.direction), 0.01f, -1.0f, 1.0f);
}

}