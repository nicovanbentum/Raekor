#include "pch.h"
#include "inspectorWidget.h"
#include "viewportWidget.h"
#include "NodeGraphWidget.h"
#include "editor.h"
#include "Raekor/OS.h"

namespace Raekor {

RTTI_CLASS_CPP_NO_FACTORY(InspectorWidget) {}


InspectorWidget::InspectorWidget(Editor* editor) : IWidget(editor, ICON_FA_INFO_CIRCLE " Inspector ") {}


void InspectorWidget::draw(float dt) {
    ImGui::Begin(m_Title.c_str(), &m_Open);
    m_Visible = ImGui::IsWindowAppearing();

    auto viewport_widget  = m_Editor->GetWidget<ViewportWidget>();
    auto nodegraph_widget = m_Editor->GetWidget<NodeGraphWidget>();

    if (viewport_widget && viewport_widget->IsVisible()) {
        DrawEntityInspector();
    }
    else if (nodegraph_widget && nodegraph_widget->IsVisible()) {
        DrawJSONInspector();
    }

    ImGui::End();
};


void InspectorWidget::DrawEntityInspector() {
    auto& active_entity = GetActiveEntity();

    if (active_entity == entt::null)
        return;

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

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 4.0f);
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

    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
}


void InspectorWidget::DrawJSONInspector() {
    ImGui::Text("JSON Inspector");

    auto nodegraph_widget = m_Editor->GetWidget<NodeGraphWidget>();
    if (!nodegraph_widget)
        return;

    auto json_object = nodegraph_widget->GetSelectedObject();
    if (!json_object)
        return;

    for (auto& [name, value] : *json_object) {
        if (name == "Type")
            continue;

        switch (value.mType) {
            case JSON::ValueType::Bool: {
                auto& bool_value = value.As<bool>();
                ImGui::Checkbox(name.c_str(), &bool_value);
            } break;
            case JSON::ValueType::String: {
                auto string_value = value.As<JSON::String>().ToString();
                ImGui::InputText(name.c_str(), &string_value);
            } break;
            case JSON::ValueType::Number: {
                auto& float_value = value.As<float>();
                ImGui::InputFloat(name.c_str(), &float_value);
            } break;
        }
    }
}


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

    ImGui::DragFloat("LOD Fade", &component.mLODFade, 0.001f, -1.0f, 1.0f, "%.3f");

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
    } else
        ImGui::Text("No material");

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_mesh_material"))
            component.material = *reinterpret_cast<const entt::entity*>(payload->Data);

        ImGui::EndDragDropTarget();
    }
}


void InspectorWidget::DrawComponent(BoxCollider& component, Entity& active) {
    auto& body_interface = GetPhysics().GetSystem().GetBodyInterface();

    ImGui::Text("Unique Body ID: %i", component.bodyID.GetIndexAndSequenceNumber());

    constexpr auto items = std::array { "Static", "Kinematic", "Dynamic" };

    auto index = int(component.motionType);
    if (ImGui::Combo("##MotionType", &index, items.data(), int(items.size()))) {
        component.motionType = JPH::EMotionType(index);
        body_interface.SetMotionType(component.bodyID, component.motionType, JPH::EActivation::Activate);
    }
}


void InspectorWidget::DrawComponent(Skeleton& component, Entity& active) {
    static auto playing = false;
    const  auto currentTime = component.animations[0].GetRunningTime();
    const  auto totalDuration = component.animations[0].GetTotalDuration();

    ImGui::ProgressBar(currentTime / totalDuration);

    if (ImGui::Button(playing ? "pause" : "play"))
        playing = !playing;

    ImGui::SameLine();

    if (ImGui::BeginCombo("Animation##InspectorWidget::drawComponent", component.animations[0].GetName().c_str())) {
        for (auto& animation : component.animations)
            ImGui::Selectable(animation.GetName().c_str());

        ImGui::EndCombo();
    }

    if (ImGui::Button("Save as graph..")) {
        const auto file_path = OS::sSaveFileDialog("DOT File (*.dot)\0", "dot");

        if (!file_path.empty()) {
            auto ofs = std::ofstream(file_path);

            ofs << "digraph G {\n";

            auto traverse = [&](auto&& traverse, const Bone& boneNode) -> void
            {
                for (const auto& child : boneNode.children) {
                    ofs << "\"" << boneNode.name << "\" -> \"" << child.name << "\";\n";
                    traverse(traverse, child);
                }
            };

            traverse(traverse, component.boneHierarchy);
            ofs << "}";
        }
    }

}


void InspectorWidget::DrawComponent(Material& component, Entity& active) {
    auto& io = ImGui::GetIO();
    auto& style = ImGui::GetStyle();
    const auto lineHeight = io.FontDefault->FontSize;

    ImGui::ColorEdit4("Albedo", glm::value_ptr(component.albedo), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    ImGui::ColorEdit3("Emissive", glm::value_ptr(component.emissive), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);

    const auto adjustedMetallic = ImGui::DragFloat("Metallic", &component.metallic, 0.001f, 0.0f, 1.0f);
    const auto adjustedRoughness = ImGui::DragFloat("Roughness", &component.roughness, 0.001f, 0.0f, 1.0f);

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
        const auto image = gpuMap ? gpuMap : Material::Default.gpuAlbedoMap;
        
        if (ImGui::ImageButton((void*)((intptr_t)image), ImVec2(lineHeight - 1, lineHeight - 1))) {
            auto filepath = OS::sOpenFileDialog("Image Files(*.jpg, *.jpeg, *.png)\0*.jpg;*.jpeg;*.png\0");

            if (!filepath.empty()) {
                const auto asset_path = TextureAsset::sConvert(filepath);

                if (!asset_path.empty()) {
                    file = asset_path;
                    gpuMap = GLRenderer::sUploadTextureFromAsset(GetAssets().GetAsset<TextureAsset>(asset_path));
                }
                else
                    ImGui::OpenPopup("Error");
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
    drawTextureInteraction(component.gpuAlbedoMap ? component.gpuAlbedoMap : Material::Default.gpuAlbedoMap, component.albedoFile);
    ImGui::AlignTextToFramePadding(); ImGui::Text("Normal Map"); ImGui::SameLine();
    drawTextureInteraction(component.gpuNormalMap ? component.gpuNormalMap : Material::Default.gpuNormalMap, component.normalFile);
    ImGui::AlignTextToFramePadding(); ImGui::Text("Material Map"); ImGui::SameLine();
    drawTextureInteraction(component.gpuMetallicRoughnessMap ? component.gpuMetallicRoughnessMap : Material::Default.gpuMetallicRoughnessMap, component.metalroughFile);

    ImGui::Checkbox("Is Transparent", &component.isTransparent);
}


bool dragVec3(const char* label, glm::vec3& v, float step, float min, float max, const char* format = "%.2f") {
    const auto window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const auto& g = *GImGui;
    auto value_changed = false;
    ImGui::PushID(label);
    ImGui::PushMultiItemsWidths(v.length(), ImGui::CalcItemWidth());
    
    constexpr auto chars = std::array { "X", "Y", "Z", "W" };

    static const auto colors = std::array {
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

        const auto type = ImGuiDataType_Float;
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

    const auto label_end = ImGui::FindRenderedTextEnd(label);
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
        const auto filepath = OS::sOpenFileDialog("DLL Files (*.dll)\0*.dll\0");
        if (!filepath.empty()) {
            component.file  = FileSystem::relative(filepath).string();
            component.asset = assets.GetAsset<ScriptAsset>(component.file);
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
        const auto filepath = OS::sOpenFileDialog("C++ Files (*.cpp)\0*.cpp\0");

        if (!filepath.empty()) {
            const auto procAddress = component.procAddress;
            
            delete component.script;
            assets.Release(component.file);
            component = NativeScript();
            
            const auto asset_path =  ScriptAsset::sConvert(filepath);
            
            if (!asset_path.empty()) {
                component.file = asset_path;
                component.procAddress = procAddress;
                component.asset = assets.GetAsset<ScriptAsset>(asset_path);

                scene.BindScriptToEntity(active, component);
            }
        }
    }

    if (ImGui::InputText("Function", &component.procAddress, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (component.asset)
            scene.BindScriptToEntity(active, component);
    }
}


void InspectorWidget::DrawComponent(DirectionalLight& component, Entity& active) {
    ImGui::ColorEdit4("Colour", glm::value_ptr(component.colour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    ImGui::DragFloat3("Direction", glm::value_ptr(component.direction), 0.01f, -1.0f, 1.0f);
}

}