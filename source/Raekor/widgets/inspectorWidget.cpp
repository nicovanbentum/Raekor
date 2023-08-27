#include "pch.h"
#include "inspectorWidget.h"
#include "components.h"
#include "viewportWidget.h"
#include "NodeGraphWidget.h"
#include "OS.h"
#include "physics.h"
#include "application.h"


namespace Raekor {

RTTI_CLASS_CPP_NO_FACTORY(InspectorWidget) {}


InspectorWidget::InspectorWidget(Application* inApp) : IWidget(inApp, reinterpret_cast<const char*>(ICON_FA_INFO_CIRCLE " Inspector ")) {}


void InspectorWidget::Draw(Widgets* inWidgets, float inDeltaTime) {
    ImGui::Begin(m_Title.c_str(), &m_Open);
    m_Visible = ImGui::IsWindowAppearing();

    auto viewport_widget  = inWidgets->GetWidget<ViewportWidget>();
    auto nodegraph_widget = inWidgets->GetWidget<NodeGraphWidget>();

    if (viewport_widget && viewport_widget->IsVisible()) {
        DrawEntityInspector(inWidgets);
    }
    else if (nodegraph_widget && nodegraph_widget->IsVisible()) {
        DrawJSONInspector(inWidgets);
    }


    ImGui::End();
};

struct EdgeHash {
public:
    std::size_t operator()(const std::pair<uint32_t, uint32_t>& x) const
    {
        return std::size_t(x.first) & std::size_t(x.second) << 32;
    }
};

void InspectorWidget::DrawEntityInspector(Widgets* inWidgets) {
    auto active_entity = m_Editor->GetActiveEntity();
    if (active_entity == NULL_ENTITY)
        return;

    ImGui::Text("Entity ID: %i", active_entity);

    auto& scene = GetScene();
    auto& assets = GetAssets();

    // I much prefered the for_each_tuple_element syntax tbh
    std::apply([&](const auto& ... components) {
        (..., [&](Assets& assets, Scene& scene, Entity& entity) {
            using ComponentType = typename std::decay<decltype(components)>::type::type;

    if (scene.Has<ComponentType>(entity)) {
        bool is_open = true;
        if (ImGui::CollapsingHeader(components.name, &is_open, ImGuiTreeNodeFlags_DefaultOpen)) {
            if (is_open)
                DrawComponent(scene.Get<ComponentType>(entity));
            else
                scene.Remove<ComponentType>(entity);
        }
    }
            }(assets, scene, active_entity));
        }, Components);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 4.0f);

    if (ImGui::BeginPopup("Components")) {
        if (ImGui::Selectable("Native Script", false)) {
            scene.Add<NativeScript>(active_entity);
            ImGui::CloseCurrentPopup();
        }

        if (scene.Has<Transform, Mesh>(active_entity)) {
            const auto& [transform, mesh] = scene.Get<Transform, Mesh>(active_entity);

            if (ImGui::Selectable("Box Collider", false)) {
                auto& collider = scene.Add<BoxCollider>(active_entity);

                const auto half_extent = glm::abs(mesh.aabb[1] - mesh.aabb[0]) / 2.0f * transform.scale;
                collider.settings.mHalfExtent = JPH::Vec3(half_extent.x, half_extent.y, half_extent.z);
                //collider.settings.SetEmbedded();
            }

            if (ImGui::Selectable("Soft Body", false)) {
                auto& soft_body = scene.Add<SoftBody>(active_entity);
                auto& transform = scene.Get<Transform>(active_entity);

                // verts
                for (const auto& [index, pos] : gEnumerate(mesh.positions)) {
                    const auto wpos = pos * transform.scale;

                    JPH::SoftBodySharedSettings::Vertex v;
                    v.mPosition = JPH::Float3(wpos.x, wpos.y, wpos.z);

                    soft_body.mSharedSettings.mVertices.push_back(v);
                }

                // faces
                for (auto index = 0u; index < mesh.indices.size(); index += 3) {
                    JPH::SoftBodySharedSettings::Face face;
                    face.mVertex[0] = mesh.indices[index];
                    face.mVertex[1] = mesh.indices[index + 1];
                    face.mVertex[2] = mesh.indices[index + 2];

                    soft_body.mSharedSettings.AddFace(face);
                }

                const auto ntriangles = mesh.indices.size() / 3;
                int		maxidx = 0;
                int i, j, ni;

                for (auto index : mesh.indices)
                    maxidx = glm::max((int)index, maxidx);
                ++maxidx;

                std::vector<bool> chks;
                chks.resize(maxidx * maxidx, false);
                
                for (int i = 0, ni = ntriangles * 3; i < ni; i += 3)
                {
                    const int idx[] = { (int)mesh.indices[i], (int)mesh.indices[i + 1], (int)mesh.indices[i + 2] };
#define IDX(_x_,_y_) ((_y_)*maxidx+(_x_))
                    for (int j = 2, k = 0; k < 3; j = k++)
                    {
                        if (!chks[IDX(idx[j], idx[k])])
                        {
                            chks[IDX(idx[j], idx[k])] = true;
                            chks[IDX(idx[k], idx[j])] = true;
                            soft_body.mSharedSettings.mEdgeConstraints.push_back(JPH::SoftBodySharedSettings::Edge(idx[j], idx[k], 0.00001f));
                        }
                    }
#undef IDX
                }

                //// edges
                //for (auto index = 0u; index < 24; index += 3) {
                //    JPH::SoftBodySharedSettings::Edge edge;
                //    edge.mCompliance = 0.00001f;
                //    
                //    edge.mVertex[0] = mesh.indices[index];
                //    edge.mVertex[1] = mesh.indices[index + 1];
                //    soft_body.mSharedSettings.mEdgeConstraints.push_back(edge);

                //    edge.mVertex[0] = mesh.indices[index];
                //    edge.mVertex[1] = mesh.indices[index + 2];
                //    soft_body.mSharedSettings.mEdgeConstraints.push_back(edge);

                //    edge.mVertex[0] = mesh.indices[index + 1];
                //    edge.mVertex[1] = mesh.indices[index + 2];
                //    soft_body.mSharedSettings.mEdgeConstraints.push_back(edge);
                //}

              

                soft_body.mSharedSettings.CalculateEdgeLengths();

                soft_body.mCreationSettings.mUpdatePosition = false;
                soft_body.mCreationSettings.mGravityFactor = 0.0f;
                soft_body.mCreationSettings.mLinearDamping = 0.0f;
                soft_body.mCreationSettings.mMaxLinearVelocity = 5.0f;

                soft_body.mSharedSettings.SetEmbedded();
            }
        }

        ImGui::EndPopup();
    }

    // Broken for now
    if (ImGui::Button("Add Component", ImVec2(ImGui::GetWindowWidth(), 0)))
        ImGui::OpenPopup("Components");

    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
}


void InspectorWidget::DrawJSONInspector(Widgets* inWidgets) {
    ImGui::Text("JSON Inspector");
}


void InspectorWidget::DrawComponent(Name& inName) {
    ImGui::InputText("Name##1", &inName.name, ImGuiInputTextFlags_AutoSelectAll);
}


void InspectorWidget::DrawComponent(Node& ioNode) {
    if (ioNode.parent != NULL_ENTITY)
        ImGui::Text("Parent entity: %i", ioNode.parent);
    else
        ImGui::Text("Parent entity: NULL");

    ImGui::Text("Siblings: %i, %i", ioNode.prevSibling, ioNode.nextSibling);
}


void InspectorWidget::DrawComponent(Mesh& ioMesh) {
    const auto byte_size = sizeof(Mesh) + ioMesh.GetInterleavedStride() * ioMesh.positions.size();
    ImGui::Text("%.1f Kb", float(byte_size) / 1024);

    ImGui::Text("%i Triangles", ioMesh.indices.size() / 3);

    ImGui::DragFloat("LOD Fade", &ioMesh.mLODFade, 0.001f, -1.0f, 1.0f, "%.3f");

    auto& scene = GetScene();
    if (scene.IsValid(ioMesh.material) && scene.Has<Material, Name>(ioMesh.material)) {
        auto [material, name] = scene.Get<Material, Name>(ioMesh.material);

        const auto albedo_imgui_id = m_Editor->GetRenderer()->GetImGuiTextureID(material.gpuAlbedoMap);
        const auto preview_size = ImVec2(10 * ImGui::GetWindowDpiScale(), 10 * ImGui::GetWindowDpiScale());
        const auto tint_color = ImVec4(material.albedo.r, material.albedo.g, material.albedo.b, material.albedo.a);

        if (ImGui::ImageButton((void*)(intptr_t)albedo_imgui_id, preview_size))
            SetActiveEntity(ioMesh.material);

        ImGui::SameLine();
        ImGui::Text(name.name.c_str());
    } else
        ImGui::Text("No material");

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_mesh_material"))
            ioMesh.material = *reinterpret_cast<const Entity*>(payload->Data);

        ImGui::EndDragDropTarget();
    }
}


void InspectorWidget::DrawComponent(SoftBody& ioSoftBody) {
    auto& body_interface = GetPhysics().GetSystem()->GetBodyInterface();

    ImGui::Text("Unique Body ID: %i", ioSoftBody.mBodyID.GetIndexAndSequenceNumber());

    if (ImGui::Button("Add Impulse")) {
        GetPhysics().GetSystem()->GetBodyInterface().AddImpulse(ioSoftBody.mBodyID, JPH::Vec3(0.0f, 0.0f, 1.0f));
    }
}


void InspectorWidget::DrawComponent(BoxCollider& inBoxCollider) {
    auto& body_interface = GetPhysics().GetSystem()->GetBodyInterface();

    ImGui::Text("Unique Body ID: %i", inBoxCollider.bodyID.GetIndexAndSequenceNumber());

    constexpr auto items = std::array { "Static", "Kinematic", "Dynamic" };

    auto index = int(inBoxCollider.motionType);
    if (ImGui::Combo("##MotionType", &index, items.data(), int(items.size()))) {
        inBoxCollider.motionType = JPH::EMotionType(index);
        body_interface.SetMotionType(inBoxCollider.bodyID, inBoxCollider.motionType, JPH::EActivation::Activate);
    }
}


void InspectorWidget::DrawComponent(Skeleton& inSkeleton) {
    static auto playing = false;
    const  auto currentTime = inSkeleton.animations[0].GetRunningTime();
    const  auto totalDuration = inSkeleton.animations[0].GetTotalDuration();

    ImGui::ProgressBar(currentTime / totalDuration);

    if (ImGui::Button(playing ? "pause" : "play"))
        playing = !playing;

    ImGui::SameLine();

    if (ImGui::BeginCombo("Animation##InspectorWidget::drawComponent", inSkeleton.animations[0].GetName().c_str())) {
        for (auto& animation : inSkeleton.animations)
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

            traverse(traverse, inSkeleton.boneHierarchy);
            ofs << "}";
        }
    }

}


void InspectorWidget::DrawComponent(Material& inMaterial) {
    auto& io = ImGui::GetIO();
    auto& style = ImGui::GetStyle();
    const auto line_height = io.FontDefault->FontSize;

    ImGui::ColorEdit4("Albedo", glm::value_ptr(inMaterial.albedo), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    ImGui::ColorEdit3("Emissive", glm::value_ptr(inMaterial.emissive), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    
    ImGui::DragFloat("Metallic", &inMaterial.metallic, 0.001f, 0.0f, 1.0f);
    ImGui::DragFloat("Roughness", &inMaterial.roughness, 0.001f, 0.0f, 1.0f);

    if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {

        ImGui::Text("Asset conversion failed. See console output log.\n");

        if (ImGui::Button("OK"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    auto DrawTextureInteraction = [&](uint32_t& inGpuMap, std::string& inFile) {
        const auto imgui_id = m_Editor->GetRenderer()->GetImGuiTextureID(inGpuMap);
        
        if (ImGui::ImageButton((void*)((intptr_t)imgui_id), ImVec2(line_height - 1, line_height - 1))) {
            auto filepath = OS::sOpenFileDialog("Image Files(*.jpg, *.jpeg, *.png)\0*.jpg;*.jpeg;*.png\0");

            if (!filepath.empty()) {
                const auto asset_path = TextureAsset::sConvert(filepath);

                if (!asset_path.empty()) {
                    inFile = asset_path;
                    const auto is_srgb = inGpuMap == inMaterial.gpuAlbedoMap;
                    inGpuMap = m_Editor->GetRenderer()->UploadTextureFromAsset(GetAssets().GetAsset<TextureAsset>(asset_path), is_srgb);
                }
                else
                    ImGui::OpenPopup("Error");
            }
        }

        if (!inFile.empty()) {
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::Text(inFile.c_str());

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(inFile.c_str());
        }
    };

    ImGui::AlignTextToFramePadding(); ImGui::Text("Albedo Map"); ImGui::SameLine();
    DrawTextureInteraction(inMaterial.gpuAlbedoMap ? inMaterial.gpuAlbedoMap : Material::Default.gpuAlbedoMap, inMaterial.albedoFile);
    ImGui::AlignTextToFramePadding(); ImGui::Text("Normal Map"); ImGui::SameLine();
    DrawTextureInteraction(inMaterial.gpuNormalMap ? inMaterial.gpuNormalMap : Material::Default.gpuNormalMap, inMaterial.normalFile);
    ImGui::AlignTextToFramePadding(); ImGui::Text("Material Map"); ImGui::SameLine();
    DrawTextureInteraction(inMaterial.gpuMetallicRoughnessMap ? inMaterial.gpuMetallicRoughnessMap : Material::Default.gpuMetallicRoughnessMap, inMaterial.metalroughFile);

    ImGui::Checkbox("Is Transparent", &inMaterial.isTransparent);
}


bool DragVec3(const char* label, glm::vec3& v, float step, float min, float max, const char* format = "%.2f") {
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


void InspectorWidget::DrawComponent(Transform& inTransform) {
    if (DragVec3("Scale", inTransform.scale, 0.001f, 0.0f, FLT_MAX))
        inTransform.Compose();

    auto degrees = glm::degrees(glm::eulerAngles(inTransform.rotation));

    if (DragVec3("Rotation", degrees, 0.1f, -360.0f, 360.0f)) {
        inTransform.rotation = glm::quat(glm::radians(degrees));
        inTransform.Compose();
    }

    if (DragVec3("Position", inTransform.position, 0.001f, -FLT_MAX, FLT_MAX))
        inTransform.Compose();
}


void InspectorWidget::DrawComponent(PointLight& inPointLight) {
    ImGui::ColorEdit4("Colour", glm::value_ptr(inPointLight.colour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
}


void InspectorWidget::DrawComponent(NativeScript& inNativeScript) {
    auto& scene = GetScene();
    auto& assets = GetAssets();

    if (inNativeScript.asset) {
        ImGui::Text("Module: %i", inNativeScript.asset->GetModule());
        ImGui::Text("File: %s", inNativeScript.file.c_str());
    }

    if (ImGui::Button("Load..")) {
        const auto filepath = OS::sOpenFileDialog("DLL Files (*.dll)\0*.dll\0");
        if (!filepath.empty()) {
            inNativeScript.file  = fs::relative(filepath).string();
            inNativeScript.asset = assets.GetAsset<ScriptAsset>(inNativeScript.file);
        }

        // component.asset->EnumerateSymbols();
    }

    ImGui::SameLine();

    if (ImGui::Button("Release")) {
        assets.Release(inNativeScript.file);
        delete inNativeScript.script;
        inNativeScript = {};
    }

    if (ImGui::InputText("Function", &inNativeScript.procAddress, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (inNativeScript.asset)
            scene.BindScriptToEntity(GetActiveEntity(), inNativeScript);
    }
}


void InspectorWidget::DrawComponent(DirectionalLight& inDirectionalLight) {
    ImGui::ColorEdit4("Colour", glm::value_ptr(inDirectionalLight.colour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    ImGui::DragFloat3("Direction", glm::value_ptr(inDirectionalLight.direction), 0.01f, -1.0f, 1.0f);
}

}