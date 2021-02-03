#include "pch.h"
#include "gui.h"
#include "systems.h"
#include "platform/OS.h"
#include "application.h"
#include "mesh.h"

#include "IconsFontAwesome5.h"

namespace Raekor {
namespace gui {

void InspectorWindow::draw(entt::registry& scene, entt::entity& entity) {
    ImGui::Begin("Inspector");

    if (entity == entt::null) {
        ImGui::End();
        return;
    }

    ImGui::Text("ID: %i", entity);

    // I much prefered the for_each_tuple_element syntax, I'll leave one of both in
    std::apply([this, &scene, &entity](const auto&... components) { (...,
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
    }(scene, entity));
    }, ecs::Components);

    if (ImGui::BeginPopup("Components")) {
        //for_each_tuple_element(ecs::Components, [&](auto component) {
        //    using ComponentType = decltype(component)::type;

        //    if (!scene.has<ComponentType>(entity)) {
        //        if (ImGui::Selectable(component.name, false)) {
        //            scene.emplace<ComponentType>(entity);
        //            ImGui::CloseCurrentPopup();
        //        }
        //    }
        //});

        if(ImGui::Selectable("Native Script", false)) {
            scene.emplace<ecs::NativeScriptComponent>(entity);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Broken for now
    if (ImGui::Button("Add Component", ImVec2(ImGui::GetWindowWidth(), 0))) {
        ImGui::OpenPopup("Components");
    }

    ImGui::End();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void InspectorWindow::drawComponent(ecs::NameComponent& component, entt::registry& scene, entt::entity& active) {
    ImGui::InputText("Name##1", &component.name, ImGuiInputTextFlags_AutoSelectAll);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void InspectorWindow::drawComponent(ecs::NodeComponent& component, entt::registry& scene, entt::entity& active) {
    ImGui::Text("Parent entity: %i", component.parent);
    ImGui::Text("Siblings: %i, %i", component.prevSibling, component.nextSibling);

    ImGui::SameLine();
    ImGui::Text("| Child count: %i", component.childCount);
}

void InspectorWindow::drawComponent(ecs::TransformComponent& component, entt::registry& scene, entt::entity& active) {
    if (ImGui::DragFloat3("Scale", glm::value_ptr(component.scale), 0.001f, 0.0f, FLT_MAX)) {
        component.recalculateMatrix();
    }
    if (ImGui::DragFloat3("Rotation", glm::value_ptr(component.rotation), 0.001f, static_cast<float>(-M_PI), static_cast<float>(M_PI))) {
        component.recalculateMatrix();
    }
    if (ImGui::DragFloat3("Position", glm::value_ptr(component.position), 0.001f, FLT_MIN, FLT_MAX)) {
        component.recalculateMatrix();
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void InspectorWindow::drawComponent(ecs::MeshComponent& component, entt::registry& scene, entt::entity& active) {
    ImGui::Text("Triangle count: %i", component.indices.size() / 3);

    if (scene.valid(component.material) && scene.has<ecs::MaterialComponent, ecs::NameComponent>(component.material)) {
        auto& [material, name] = scene.get<ecs::MaterialComponent, ecs::NameComponent>(component.material);

        const auto albedoTexture = (void*)((intptr_t)material.albedo);
        const auto previewSize = ImVec2(10 * ImGui::GetWindowDpiScale(), 10 * ImGui::GetWindowDpiScale());
        const auto tintColor = ImVec4(material.baseColour.r, material.baseColour.g, material.baseColour.b, material.baseColour.a);

        if (ImGui::ImageButton(albedoTexture, previewSize, ImVec2(0,0), ImVec2(1,1), -1, ImVec4(0,0,0,0), tintColor)) {
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

//////////////////////////////////////////////////////////////////////////////////////////////////

void InspectorWindow::drawComponent(ecs::MaterialComponent& component, entt::registry& scene, entt::entity& active) {
    auto& io = ImGui::GetIO();
    auto& style = ImGui::GetStyle();
    float lineHeight = io.FontDefault->FontSize;

    ImGui::ColorEdit4("Base colour", glm::value_ptr(component.baseColour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);

    const bool adjustedMetallic = ImGui::DragFloat("Metallic", &component.metallic, 0.001f, 0.0f, 1.0f);
    const bool adjustedRoughness = ImGui::DragFloat("Roughness", &component.roughness, 0.001f, 0.0f, 1.0f);

    if (adjustedMetallic || adjustedRoughness) {
        if (component.mrFile.empty() && component.metalrough) {
            auto metalRoughnessValue = glm::vec4(component.metallic, component.roughness, 0.0f, 1.0f);
            glTextureSubImage2D(component.metalrough, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, glm::value_ptr(metalRoughnessValue));
        }
    }

    if (component.albedo) {
        ImGui::Image((void*)((intptr_t)component.albedo), ImVec2(lineHeight, lineHeight));
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
            component.createAlbedoTexture(image);
        }
    }

    if (component.normals) {
        ImGui::Image((void*)((intptr_t)component.normals), ImVec2(lineHeight, lineHeight));
        ImGui::SameLine();
    }
    
    ImGui::Text("Normal map");
    ImGui::SameLine();
    if (ImGui::SmallButton("... ## normalmap")) {
        std::string filepath = OS::openFileDialog(fileFilters);
        if (!filepath.empty()) {
            Stb::Image image;
            image.load(filepath, true);
            component.createNormalTexture(image);
        }
    }

    if (component.metalrough) {
        ImGui::Image((void*)((intptr_t)component.metalrough), ImVec2(lineHeight, lineHeight));
        ImGui::SameLine();
    }

    ImGui::Text("Metallic Roughness map");
    ImGui::SameLine();
    if (ImGui::SmallButton("... ## metallic roughness")) {
        std::string filepath = OS::openFileDialog(fileFilters);
    }

}

//////////////////////////////////////////////////////////////////////////////////////////////////

void InspectorWindow::drawComponent(ecs::PointLightComponent& component, entt::registry& scene, entt::entity& active) {
    ImGui::ColorEdit4("Colour", glm::value_ptr(component.buffer.colour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void InspectorWindow::drawComponent(ecs::DirectionalLightComponent& component, entt::registry& scene, entt::entity& active) {
    ImGui::ColorEdit4("Colour", glm::value_ptr(component.buffer.colour), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    ImGui::DragFloat3("Direction", glm::value_ptr(component.buffer.direction), 0.01f, -1.0f, 1.0f);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void InspectorWindow::drawComponent(ecs::MeshAnimationComponent& component, entt::registry& scene, entt::entity& active) {
    static bool playing = false;
    ImGui::SliderFloat("Time", &component.animation.runningTime, 0, component.animation.totalDuration);
    if (ImGui::Button(playing ? "pause" : "play")) {
        playing = !playing;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void InspectorWindow::drawComponent(ecs::NativeScriptComponent& component, entt::registry& scene, entt::entity& active) {
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

//////////////////////////////////////////////////////////////////////////////////////////////////

bool EntityWindow::drawFamilyNode(entt::registry& scene, entt::entity entity, entt::entity& active) {
    auto& node = scene.get<ecs::NodeComponent>(entity);

    auto selected = active == entity ? ImGuiTreeNodeFlags_Selected : 0;
    auto treeNodeFlags = selected | ImGuiTreeNodeFlags_OpenOnArrow;
    auto name = scene.get<ecs::NameComponent>(entity);
    bool opened = ImGui::TreeNodeEx(name.name.c_str(), treeNodeFlags);
    if (ImGui::IsItemClicked()) {
        active = active == entity ? entt::null : entity;
    }
     return opened;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void EntityWindow::drawChildlessNode(entt::registry& scene, entt::entity entity, entt::entity& active) {
    auto& node = scene.get<ecs::NodeComponent>(entity);
    auto name = scene.get<ecs::NameComponent>(entity);
    if (ImGui::Selectable(std::string(name.name + "##" + std::to_string(static_cast<uint32_t>(entity))).c_str(), entity == active)) {
        active = active == entity ? entt::null : entity;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void EntityWindow::drawFamily(entt::registry& scene, entt::entity entity, entt::entity& active) {
    auto& node = scene.get<ecs::NodeComponent>(entity);
    auto currentChild = node.firstChild;

    for (unsigned int i = 0; i < node.childCount; i++) {
        auto& currentChildComponent = scene.get<ecs::NodeComponent>(currentChild);
    
        if (currentChildComponent.childCount > 0) {
            if (drawFamilyNode(scene, currentChild, active)) {
                drawFamily(scene, currentChild, active);
                ImGui::TreePop();
            }
        } else {
            drawChildlessNode(scene, currentChild, active);
        }

        currentChild = scene.get<ecs::NodeComponent>(currentChild).nextSibling;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void EntityWindow::draw(entt::registry& scene, entt::entity& active) {
    ImGui::Begin("Scene");

    auto nodeView = scene.view<ecs::NodeComponent>();
    for (auto entity : nodeView) {
        auto& node = nodeView.get<ecs::NodeComponent>(entity);

        if (node.parent == entt::null) {
            if (node.childCount > 0) {
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

//////////////////////////////////////////////////////////////////////////////////////////////////

void Guizmo::drawWindow() {
    ImGui::Begin("Editor");

    auto totalContentRegionAvail = ImGui::GetContentRegionAvail();

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

    float currentWidth = totalContentRegionAvail.x - ImGui::GetContentRegionAvail().x;
    float advanceWidth = (totalContentRegionAvail.x / 2) - currentWidth;

    ImGui::SameLine(advanceWidth - ImGui::GetFrameHeightWithSpacing() * 1.5f);

    ImGui::Button(ICON_FA_HAMMER);
    ImGui::SameLine();
    ImGui::Button(ICON_FA_PLAY);
    ImGui::SameLine();
    ImGui::Button(ICON_FA_STOP);

    ImGui::End();
}

ImGuizmo::OPERATION Guizmo::getOperation() { return operation; }

//////////////////////////////////////////////////////////////////////////////////////////////////

void AssetWindow::drawWindow(entt::registry& assets, entt::entity& active) {
    ImGui::Begin("Asset Browser");

    auto materialView = assets.view<ecs::MaterialComponent, ecs::NameComponent>();
    ImGui::Columns(10);
    for (auto entity : materialView) {
        auto& [material, name] = materialView.get<ecs::MaterialComponent, ecs::NameComponent>(entity);
        std::string selectableName = name.name.c_str() + std::string("##") + std::to_string(entt::to_integral(entity));
        if (ImGui::Selectable(selectableName.c_str() , active == entity)) {
            active = entity;
        }

        ImGuiDragDropFlags src_flags = ImGuiDragDropFlags_SourceNoDisableHover;
        src_flags |= ImGuiDragDropFlags_SourceNoHoldToOpenOthers;
        if (ImGui::BeginDragDropSource(src_flags)) {
            ImGui::SetDragDropPayload("drag_drop_mesh_material", &entity, sizeof(entt::entity));
            ImGui::EndDragDropSource();
        }

        ImGui::NextColumn();
    }

    ImGui::End();

}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Dockspace::begin() {
    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    // because it would be confusing to have two docking targets within each others.
    ImGuiWindowFlags dockWindowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiViewport* imGuiViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(imGuiViewport->Pos);
    ImGui::SetNextWindowSize(imGuiViewport->Size);
    ImGui::SetNextWindowViewport(imGuiViewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    dockWindowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the pass-thru hole, 
    // so we ask Begin() to not render a background.
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) dockWindowFlags |= ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", (bool*)true, dockWindowFlags);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);

    // DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Dockspace::end() {
    ImGui::End();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

bool ViewportWindow::draw(Viewport& viewport, GLRenderer& renderer, entt::registry& scene, entt::entity& active) {
    // renderer viewport
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Renderer", NULL, ImGuiWindowFlags_AlwaysAutoResize);

    // figure out if we need to resize the viewport
    auto size = ImGui::GetContentRegionAvail();
    auto resized = false;
    if (viewport.size.x != size.x || viewport.size.y != size.y) {
        viewport.resize({ size.x, size.y });
        resized = true;
    }

    // render the active screen texture to the view port as an imgui image
    ImGui::Image((void*)((intptr_t)texture), ImVec2((float)viewport.size.x, (float)viewport.size.y), { 0,1 }, { 1,0 });

    // the viewport image is a drag and drop target for dropping materials onto meshes
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("drag_drop_mesh_material")) {
            auto mousePos = gui::getMousePosWindow(viewport, ImGui::GetWindowPos());
            uint32_t pixel = renderer.geometryBufferPass->readEntity(mousePos.x, mousePos.y);
            entt::entity picked = static_cast<entt::entity>(pixel);

            if (scene.valid(picked)) {
                auto mesh = scene.try_get<ecs::MeshComponent>(picked);
                if (mesh) {
                    mesh->material = *reinterpret_cast<const entt::entity*>(payload->Data);
                    active = picked;
                }
            }
        }

        ImGui::EndDragDropTarget();
    }

    auto pos = ImGui::GetWindowPos();
    viewport.offset = { pos.x, pos.y };

    mouseInViewport = ImGui::IsWindowHovered();

    auto& io = ImGui::GetIO();
    if (io.MouseClicked[0] && mouseInViewport && !(active != entt::null && ImGuizmo::IsOver(gizmo.getOperation()))) {
        auto mousePos = gui::getMousePosWindow(viewport, ImGui::GetWindowPos());
        uint32_t pixel = renderer.geometryBufferPass->readEntity(mousePos.x, mousePos.y);
        entt::entity picked = static_cast<entt::entity>(pixel);

        if (scene.valid(picked)) {
            active = active == picked ? entt::null : picked;
        } else {
            active = entt::null;
        }
    }

    if (active != entt::null) {
        drawGizmo(gizmo, scene, viewport, active);
    }

    ImGui::SetNextWindowPos(ImGui::GetWindowPos());

    ImGui::End();
    ImGui::PopStyleVar();

    // application/render metrics
    ImGui::SetNextWindowBgAlpha(0.35f);
    auto metricWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize;
    ImGui::Begin("GPU Metrics", (bool*)0, metricWindowFlags);
    ImGui::Text("Vendor: %s", glGetString(GL_VENDOR));
    ImGui::Text("Product: %s", glGetString(GL_RENDERER));
    ImGui::Text("Resolution: %i x %i", viewport.size.x, viewport.size.y);
    ImGui::Text("Frame %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::Text("Graphics API: OpenGL %s", glGetString(GL_VERSION));
    ImGui::End();

    return resized;
}

void ViewportWindow::setTexture(unsigned int texture) {
    this->texture = texture;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void ViewportWindow::drawGizmo(const Guizmo& gizmo, entt::registry& scene, Viewport& viewport, entt::entity active) {
    if (!scene.valid(active) || !gizmo.enabled || !scene.has<ecs::TransformComponent>(active)) return;
    auto& transform = scene.get<ecs::TransformComponent>(active);

    // set the gizmo's viewport
    ImGuizmo::SetDrawlist();
    auto pos = ImGui::GetWindowPos();
    ImGuizmo::SetRect(pos.x, pos.y, (float)viewport.size.x, (float)viewport.size.y);

    // temporarily transform to mesh space for gizmo use
    auto mesh = scene.try_get<ecs::MeshComponent>(active);
    if (mesh) transform.matrix = glm::translate(transform.matrix, ((mesh->aabb[0] + mesh->aabb[1]) / 2.0f));

    // draw gizmo
    bool manipulated = ImGuizmo::Manipulate(
        glm::value_ptr(viewport.getCamera().getView()),
        glm::value_ptr(viewport.getCamera().getProjection()),
        gizmo.operation, ImGuizmo::MODE::WORLD,
        glm::value_ptr(transform.matrix)
    );

    // transform back to world space
    if (mesh) transform.matrix = glm::translate(transform.matrix, -((mesh->aabb[0] + mesh->aabb[1]) / 2.0f));


    // update the transformation
    if (manipulated) {
        glm::vec3 skew;
        glm::quat rotation;
        glm::vec4 perspective;
        glm::decompose(transform.matrix, transform.scale, rotation, transform.position, skew, perspective);
        glm::extractEulerAngleXYZ(transform.matrix, transform.rotation.x, transform.rotation.y, transform.rotation.z);
    }


}

//////////////////////////////////////////////////////////////////////////////////////////////////

void setTheme(const std::array<std::array<float, 4>, ImGuiCol_COUNT>& data) {
    // load the UI's theme from config
    ImVec4* colors = ImGui::GetStyle().Colors;
    for (unsigned int i = 0; i < data.size(); i++) {
        auto& savedColor = data[i];
        ImGuiCol_Text;
        colors[i] = ImVec4(savedColor[0], savedColor[1], savedColor[2], savedColor[3]);
    }

    ImGui::GetStyle().WindowRounding = 0.0f;
    ImGui::GetStyle().ChildRounding = 0.0f;
    ImGui::GetStyle().FrameRounding = 0.0f;
    ImGui::GetStyle().GrabRounding = 0.0f;
    ImGui::GetStyle().PopupRounding = 0.0f;
    ImGui::GetStyle().ScrollbarRounding = 0.0f;
    ImGui::GetStyle().WindowBorderSize = 0.0f;
    ImGui::GetStyle().ChildBorderSize = 0.0f;
    ImGui::GetStyle().FrameBorderSize = 0.0f;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void setFont(const std::string& filepath) {
    auto& io = ImGui::GetIO();
    ImFont* pFont = io.Fonts->AddFontFromFileTTF(filepath.c_str(), 15.0f);
    if (!io.Fonts->Fonts.empty()) {
        io.FontDefault = io.Fonts->Fonts.back();
    }

    // merge in icons from Font Awesome
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    ImFontConfig icons_config; icons_config.MergeMode = true; icons_config.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF("resources/" FONT_ICON_FILE_NAME_FAS, 15.0f, &icons_config, icons_ranges);
    // use FONT_ICON_FILE_NAME_FAR if you want regular instead of solid
}

glm::ivec2 getMousePosWindow(const Viewport& viewport, ImVec2 windowPos) {
    // get mouse position in window
    glm::ivec2 mousePosition;
    SDL_GetMouseState(&mousePosition.x, &mousePosition.y);

    // get mouse position relative to viewport
    glm::ivec2 rendererMousePosition = { (mousePosition.x - windowPos.x), (mousePosition.y - windowPos.y) };

    // flip mouse coords for opengl
    rendererMousePosition.y = std::max(viewport.size.y - rendererMousePosition.y, 0u);
    rendererMousePosition.x = std::max(rendererMousePosition.x, 0);

    return rendererMousePosition;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void TopMenuBar::draw(WindowApplication* app, Scene& scene, GLRenderer& renderer, entt::entity& active)
{
    // draw the top user bar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene")) {
                scene->clear();
                active = entt::null;
            }

            if (ImGui::MenuItem("Open scene..")) {
                std::string filepath = OS::openFileDialog("Scene Files (*.scene)\0*.scene\0");
                if (!filepath.empty()) {
                    SDL_SetWindowTitle(app->getWindow(), std::string(filepath + " - Raekor Renderer").c_str());
                    scene.openFromFile(filepath);
                    active = entt::null;
                }
            }

            if (ImGui::MenuItem("Save scene..", "CTRL + S")) {
                std::string filepath = OS::saveFileDialog("Scene File (*.scene)\0", "scene");
                if (!filepath.empty()) {
                    scene.saveToFile(filepath);
                }
            }

            if (ImGui::MenuItem("Load model..")) {
                std::string filepath = OS::openFileDialog("Supported Files(*.gltf, *.fbx, *.obj)\0*.gltf;*.fbx;*.obj\0");
                if (!filepath.empty()) {
                    AssimpImporter::loadFile(scene, filepath);
                    active = entt::null;
                }
            }

            if (ImGui::MenuItem("Save Screenshot..")) {
                std::string savePath = OS::saveFileDialog("Uncompressed PNG (*.png)\0", "png");

                if (!savePath.empty()) {
                    auto& viewport = app->getViewport();
                    const auto bufferSize = 4 * viewport.size.x * viewport.size.y;
                    auto pixels = std::vector<unsigned char>(bufferSize);
                    glGetTextureImage(0, 0, GL_RGBA, GL_UNSIGNED_BYTE, bufferSize * sizeof(unsigned char), pixels.data());
                    stbi_flip_vertically_on_write(true);
                    stbi_write_png(savePath.c_str(), viewport.size.x, viewport.size.y, 4, pixels.data(), viewport.size.x * 4);
                }

            }

            if (ImGui::MenuItem("Exit", "Escape")) {
                app->running = false;
            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Delete", "DEL")) {
                // on press we remove the scene object
                if (active != entt::null) {
                    if (scene->has<ecs::NodeComponent>(active)) {
                        std::stack<entt::entity> entities;
                        entities.push(active);
                        while (!entities.empty()) {
                            auto& node = scene->get<ecs::NodeComponent>(entities.top());
                            entities.pop();

                            auto current = node.firstChild;
                            while (current != entt::null) {
                                NodeSystem::remove(scene, scene->get<ecs::NodeComponent>(current));
                                current = scene->get<ecs::NodeComponent>(current).nextSibling;
                                entities.push(current);
                            }
                        }
                    }
                    else {
                        scene->destroy(active);
                    }
                    active = entt::null;
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Add")) {
            if (ImGui::MenuItem("Empty", "CTRL+E")) {
                auto entity = scene.createObject("Empty");
                active = entity;
            }
            ImGui::Separator();

            if (ImGui::MenuItem("Material")) {
                auto entity = scene->create();
                scene->emplace<ecs::NameComponent>(entity, "New Material");
                auto& defaultMaterial = scene->emplace<ecs::MaterialComponent>(entity);
                defaultMaterial.createMetalRoughTexture();
                active = entity;
            }

            if (ImGui::BeginMenu("Shapes")) {

                if (ImGui::MenuItem("Sphere")) {
                    auto entity = scene.createObject("Sphere");
                    auto& mesh = scene->emplace<ecs::MeshComponent>(entity);

                    const float radius = 2.0f;
                    float x, y, z, xy;                              // vertex position
                    float nx, ny, nz, lengthInv = 1.0f / radius;    // vertex normal
                    float s, t;                                     // vertex texCoord

                    const int sectorCount = 36;
                    const int stackCount = 18;
                    const float PI = static_cast<float>(M_PI);
                    float sectorStep = 2 * PI / sectorCount;
                    float stackStep = PI / stackCount;
                    float sectorAngle, stackAngle;

                    for (int i = 0; i <= stackCount; ++i)
                    {
                        stackAngle = PI / 2 - i * stackStep;        // starting from pi/2 to -pi/2
                        xy = radius * cosf(stackAngle);             // r * cos(u)
                        z = radius * sinf(stackAngle);              // r * sin(u)

                        // add (sectorCount+1) vertices per stack
                        // the first and last vertices have same position and normal, but different tex coords
                        for (int j = 0; j <= sectorCount; ++j)
                        {
                            sectorAngle = j * sectorStep;           // starting from 0 to 2pi

                            // vertex position (x, y, z)
                            x = xy * cosf(sectorAngle);             // r * cos(u) * cos(v)
                            y = xy * sinf(sectorAngle);             // r * cos(u) * sin(v)
                            mesh.positions.emplace_back(x, y, z);

                            // normalized vertex normal (nx, ny, nz)
                            nx = x * lengthInv;
                            ny = y * lengthInv;
                            nz = z * lengthInv;
                            mesh.normals.emplace_back(nx, ny, nz);

                            // vertex tex coord (s, t) range between [0, 1]
                            s = (float)j / sectorCount;
                            t = (float)i / stackCount;
                            mesh.uvs.emplace_back(s, t);

                        }
                    }

                    int k1, k2;
                    for (int i = 0; i < stackCount; ++i)
                    {
                        k1 = i * (sectorCount + 1);     // beginning of current stack
                        k2 = k1 + sectorCount + 1;      // beginning of next stack

                        for (int j = 0; j < sectorCount; ++j, ++k1, ++k2)
                        {
                            // 2 triangles per sector excluding first and last stacks
                            // k1 => k2 => k1+1
                            if (i != 0)
                            {
                                mesh.indices.push_back(k1);
                                mesh.indices.push_back(k2);
                                mesh.indices.push_back(k1 + 1);
                            }

                            // k1+1 => k2 => k2+1
                            if (i != (stackCount - 1))
                            {
                                mesh.indices.push_back(k1 + 1);
                                mesh.indices.push_back(k2);
                                mesh.indices.push_back(k2 + 1);
                            }
                        }
                    }

                    mesh.generateTangents();
                    mesh.uploadVertices();
                    mesh.uploadIndices();
                    mesh.generateAABB();
                }

                if (ImGui::MenuItem("Plane")) {
                    auto entity = scene.createObject("Plane");
                    auto& mesh = scene->emplace<ecs::MeshComponent>(entity);
                    for (const auto& v : planeVertices) {
                        mesh.positions.push_back(v.pos);
                        mesh.uvs.push_back(v.uv);
                        mesh.normals.push_back(v.normal);
                    }

                    for (const auto& triangle : planeIndices) {
                        mesh.indices.push_back(triangle.p1);
                        mesh.indices.push_back(triangle.p2);
                        mesh.indices.push_back(triangle.p3);
                    }

                    mesh.generateTangents();
                    mesh.uploadVertices();
                    mesh.uploadIndices();
                    mesh.generateAABB();
                }

                //if (ImGui::MenuItem("Cube")) {
                //    auto entity = scene.createObject("Cube");
                //    auto& mesh = scene->emplace<ecs::MeshComponent>(entity);

                //    if (active != entt::null) {
                //        auto& node = scene->get<ecs::NodeComponent>(entity);
                //        node.parent = active;
                //        node.hasChildren = false;
                //        scene->get<ecs::NodeComponent>(node.parent).hasChildren = true;
                //    }

                //    for (const auto& v : unitCubeVertices) {
                //        mesh.positions.push_back(v.pos);
                //        mesh.uvs.push_back(v.uv);
                //        mesh.normals.push_back(v.normal);
                //    }

                //    for (const auto& index : cubeIndices) {
                //        mesh.indices.push_back(index.p1);
                //        mesh.indices.push_back(index.p2);
                //        mesh.indices.push_back(index.p3);
                //    }

                //    mesh.generateTangents();
                //    mesh.uploadVertices();
                //    mesh.uploadIndices();
                //    mesh.generateAABB();
                //}

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Light")) {

                if (ImGui::MenuItem("Directional Light")) {
                    auto entity = scene.createObject("Directional Light");
                    auto& transform = scene->get<ecs::TransformComponent>(entity);
                    transform.rotation.x = static_cast<float>(M_PI / 12);
                    transform.recalculateMatrix();
                    scene->emplace<ecs::DirectionalLightComponent>(entity);
                    active = entity;
                }

                if (ImGui::MenuItem("Point Light")) {
                    auto entity = scene.createObject("Point Light");
                    scene->emplace<ecs::PointLightComponent>(entity);
                    active = entity;
                }

                ImGui::EndMenu();
            }


            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About", "")) {}
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void CameraSettings::drawWindow(Camera& camera) {
    ImGui::Begin("Camera Properties");
    if (ImGui::DragFloat("Move Speed", &camera.moveSpeed, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}
    if (ImGui::DragFloat("Move Constant", &camera.moveConstant, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}
    if (ImGui::DragFloat("Look Speed", &camera.lookSpeed, 0.1f, 0.0001f, FLT_MAX, "%.4f")) {}
    if (ImGui::DragFloat("Look Constant", &camera.lookConstant, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}
    if (ImGui::DragFloat("Zoom Speed", &camera.zoomSpeed, 0.001f, 0.0001f, FLT_MAX, "%.4f")) {}
    if (ImGui::DragFloat("Zoom Constant", &camera.zoomConstant, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}

    ImGui::End();
}

void RandomWindow::drawWindow(GLRenderer& renderer) {
    // scene panel
    ImGui::Begin("Random");
    ImGui::SetItemDefaultFocus();

    // toggle button for openGl vsync
    if (ImGui::RadioButton("Vsync", renderer.vsync)) {
        renderer.vsync = !renderer.vsync;
        SDL_GL_SetSwapInterval(renderer.vsync);
    }

    ImGui::NewLine(); ImGui::Separator();
    ImGui::Text("Voxel Cone Tracing");

    if (ImGui::RadioButton("Debug", renderer.debugVoxels)) {
        renderer.debugVoxels = !renderer.debugVoxels;
    }

    if (ImGui::RadioButton("Update", renderer.shouldVoxelize)) {
        renderer.shouldVoxelize = !renderer.shouldVoxelize;
    }

    ImGui::DragFloat("Range", &renderer.voxelizationPass->worldSize, 0.05f, 1.0f, FLT_MAX, "%.2f");

    ImGui::NewLine(); ImGui::Separator();
    ImGui::Text("Skydome");

    ImGui::NewLine(); ImGui::Separator();

    ImGui::NewLine();

    ImGui::Text("Shadow Mapping");
    ImGui::Separator();

    if (ImGui::DragFloat2("Planes", glm::value_ptr(renderer.shadowMapPass->settings.planes), 0.1f)) {}
    if (ImGui::DragFloat("Size", &renderer.shadowMapPass->settings.size)) {}
    if (ImGui::DragFloat("Bias constant", &renderer.shadowMapPass->settings.depthBiasConstant, 0.01f, 0.0f, FLT_MAX, "%.2f")) {}
    if (ImGui::DragFloat("Bias slope factor", &renderer.shadowMapPass->settings.depthBiasSlope, 0.01f, 0.0f, FLT_MAX, "%.2f")) {}

    ImGui::NewLine();
    ImGui::Separator();
    ImGui::NewLine();

    ImGui::End();
}

void PostprocessWindow::drawWindow(GLRenderer& renderer) {
    // post processing panel
    ImGui::Begin("Post Processing");

    ImGui::Separator();

    if (ImGui::SliderFloat("Exposure", &renderer.tonemappingPass->settings.exposure, 0.0f, 1.0f)) {}
    if (ImGui::SliderFloat("Gamma", &renderer.tonemappingPass->settings.gamma, 1.0f, 3.2f)) {}
    ImGui::NewLine();

    if (ImGui::Checkbox("Bloom", &doBloom)) {}
    ImGui::Separator();

    if (ImGui::DragFloat3("Threshold", glm::value_ptr(renderer.DeferredLightingPass->settings.bloomThreshold), 0.001f, 0.0f, 10.0f)) {}
    ImGui::NewLine();

    ImGui::End();
}

} // gui
} // raekor