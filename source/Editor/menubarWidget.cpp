#include "pch.h"
#include "menubarWidget.h"
#include "editor.h"
#include "Raekor/OS.h"
#include "Raekor/gltf.h"
#include "Raekor/assimp.h"
#include "Raekor/systems.h"
#include "Raekor/scene.h"
#include "Raekor/timer.h"

namespace Raekor {

RTTI_CLASS_CPP_NO_FACTORY(MenubarWidget) {}

MenubarWidget::MenubarWidget(Editor* editor) : 
    IWidget(editor, "Menubar"),
    m_ActiveEntity(GetActiveEntity())
{}


void MenubarWidget::draw(float dt) {
    auto& scene = IWidget::GetScene();

    if (ImGui::BeginMainMenuBar()) {
        ImGui::Text(ICON_FA_ADDRESS_BOOK);

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New scene")) {
                scene.clear();
                m_ActiveEntity = entt::null;
            }

            if (ImGui::MenuItem("Open scene..")) {
                std::string filepath = OS::sOpenFileDialog("Scene Files (*.scene)\0*.scene\0");

                if (!filepath.empty()) {
                    Timer timer;
                    SDL_SetWindowTitle(m_Editor->GetWindow(), std::string(filepath + " - Raekor Renderer").c_str());
                    scene.OpenFromFile(IWidget::GetAssets(), filepath);
                    m_ActiveEntity = entt::null;
                    std::cout << "Open scene time: " << Timer::sToMilliseconds(timer.GetElapsedTime()) << '\n';
                }
            }

            if (ImGui::MenuItem("Save scene..", "CTRL + S")) {
                std::string filepath = OS::sSaveFileDialog("Scene File (*.scene)\0", "scene");

                if (!filepath.empty())
                    scene.SaveToFile(IWidget::GetAssets(), filepath);
            }

            if (ImGui::MenuItem("Serialize as JSON..", "CTRL + S")) {
                auto folder = Path(OS::sSelectFolderDialog());

                Timer timer;
                if (!folder.empty()) {
                    for (const auto& [entity, name, mesh] : scene.view<Name, Mesh>().each()) {
                        // for the love of god C++ overlords, let me capture structured bindings
                        Async::sQueueJob([&, name = &name, mesh = &mesh]() {
                            auto ofs = std::ofstream((folder / name->name).replace_extension(".mesh"));
                            cereal::JSONOutputArchive archive(ofs);
                            archive(*mesh);
                        });
                    }

                    for (const auto& [entity, name, material] : scene.view<Name, Material>().each()) {
                        // for the love of god C++ overlords, let me capture structured bindings
                        Async::sQueueJob([&, name = &name, material = &material]() {
                            auto ofs = std::ofstream((folder / name->name).replace_extension(".material"));
                            cereal::JSONOutputArchive archive(ofs);
                            archive(*material);
                        });
                    }
                    Async::sWait();
                }

                std::clog << "Serialize scene time: " << Timer::sToMilliseconds(timer.GetElapsedTime()) << " ms.\n";
            }

            if (ImGui::MenuItem("Load model..")) {
                std::string filepath = OS::sOpenFileDialog("Supported Files(*.gltf, *.fbx, *.glb, *.obj, *.blend)\0*.gltf;*.fbx;*.glb;*.obj;*.blend\0");
                
                if (!filepath.empty()) {
                    AssimpImporter importer(scene);
                    importer.SetUploadMeshCallbackFunction(GLRenderer::sUploadMeshBuffers);
                    importer.SetUploadMaterialCallbackFunction(GLRenderer::sUploadMaterialTextures);
                    importer.SetUploadSkeletonCallbackFunction(GLRenderer::sUploadSkeletonBuffers);
                    importer.LoadFromFile(GetAssets(), filepath);
                    m_ActiveEntity = sInvalidEntity;
                }
            }

            if (ImGui::MenuItem("Load GLTF..")) {
                std::string filepath = OS::sOpenFileDialog("Supported Files(*.gltf, *.glb)\0*.gltf;*.glb\0");

                if (!filepath.empty()) {
                    GltfImporter importer(scene);
                    importer.SetUploadMeshCallbackFunction(GLRenderer::sUploadMeshBuffers);
                    importer.SetUploadMaterialCallbackFunction(GLRenderer::sUploadMaterialTextures);
                    importer.SetUploadSkeletonCallbackFunction(GLRenderer::sUploadSkeletonBuffers);
                    importer.LoadFromFile(IWidget::GetAssets(), filepath);
                    m_ActiveEntity = sInvalidEntity;
                }
            }

            if (ImGui::MenuItem("Compile script..")) {
                std::string filepath = OS::sOpenFileDialog("C++ Files (*.cpp)\0*.cpp\0");
                if (!filepath.empty()) {
                    Async::sQueueJob([filepath]() {
                        ScriptAsset::sConvert(filepath);
                    });
                }
            }

            if (ImGui::MenuItem("Save screenshot..")) {
                std::string savePath = OS::sSaveFileDialog("Uncompressed PNG (*.png)\0", "png");

                if (!savePath.empty()) {
                    auto& viewport = m_Editor->GetViewport();
                    const auto bufferSize = 4 * viewport.size.x * viewport.size.y;
                    
                    auto pixels = std::vector<unsigned char>(bufferSize);
                    glGetTextureImage(IWidget::GetRenderer().m_Tonemap->result, 0, GL_RGBA, GL_UNSIGNED_BYTE, bufferSize * sizeof(unsigned char), pixels.data());

                    stbi_flip_vertically_on_write(true);
                    stbi_write_png(savePath.c_str(), viewport.size.x, viewport.size.y, 4, pixels.data(), viewport.size.x * 4);
                }
            }

            if (ImGui::MenuItem("Exit", "Escape"))
                m_Editor->Terminate();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {

            if (ImGui::MenuItem("Delete", "DEL")) {
                if (m_ActiveEntity != sInvalidEntity) {
                    IWidget::GetScene().DestroySpatialEntity(m_ActiveEntity);
                    m_ActiveEntity = sInvalidEntity;
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::PushItemFlag(ImGuiItemFlags_SelectableDontClosePopup, true);

            for (const auto& widget : m_Editor->GetWidgets()) {
                if (widget.get() == this)
                    continue;

                bool is_visible = widget->IsVisible();

                if (ImGui::MenuItem(std::string(widget->GetTitle() + "Window").c_str(), "", &is_visible))
                    is_visible ? widget->Show() : widget->Hide();
            }

            ImGui::PopItemFlag();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Add")) {
            if (ImGui::MenuItem("Empty", "CTRL+E")) {
                auto entity = scene.CreateSpatialEntity("Empty");
                m_ActiveEntity = entity;
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Material")) {
                auto entity = scene.create();
                scene.emplace<Name>(entity, "New Material");
                scene.emplace<Material>(entity);
                m_ActiveEntity = entity;
            }

            if (ImGui::BeginMenu("Shapes")) {
                if (ImGui::MenuItem("Sphere")) {
                    auto entity = scene.CreateSpatialEntity("Sphere");
                    auto& mesh = scene.emplace<Mesh>(entity);

                    if (m_ActiveEntity != sInvalidEntity) {
                        auto& node = scene.get<Node>(entity);
                        NodeSystem::sAppend(scene, scene.get<Node>(m_ActiveEntity), node);
                    }

                    gGenerateCube(mesh, 1.0f, 16, 16);

                    GLRenderer::sUploadMeshBuffers(mesh);
                }

                if (ImGui::MenuItem("Plane")) {
                    auto entity = scene.CreateSpatialEntity("Plane");
                    auto& mesh = scene.emplace<Mesh>(entity);
                    
                    if (m_ActiveEntity != sInvalidEntity && scene.any_of<Node>(m_ActiveEntity)) {
                        auto& node = scene.get<Node>(entity);
                        NodeSystem::sAppend(scene, scene.get<Node>(m_ActiveEntity), node);
                    }
                    
                    for (const auto& v : UnitPlane::vertices) {
                        mesh.positions.push_back(v.pos);
                        mesh.uvs.push_back(v.uv);
                        mesh.normals.push_back(v.normal);
                    }

                    for (const auto& triangle : UnitPlane::indices) {
                        mesh.indices.push_back(triangle.p1);
                        mesh.indices.push_back(triangle.p2);
                        mesh.indices.push_back(triangle.p3);
                    }

                    mesh.CalculateTangents();
                    mesh.CalculateAABB();
                    GLRenderer::sUploadMeshBuffers(mesh);
                }

                if (ImGui::MenuItem("Cube")) {
                    auto entity = scene.CreateSpatialEntity("Cube");
                    auto& mesh = scene.emplace<Mesh>(entity);

                    if (m_ActiveEntity != sInvalidEntity && scene.all_of<Node>(m_ActiveEntity)) {
                        auto& node = scene.get<Node>(entity);
                        NodeSystem::sAppend(scene, scene.get<Node>(m_ActiveEntity), node);
                    }

                    for (const auto& v : UnitCube::vertices) {
                        mesh.positions.push_back(v.pos);
                        mesh.uvs.push_back(v.uv);
                        mesh.normals.push_back(v.pos);
                    }

                    for (const auto& index : UnitCube::indices) {
                        mesh.indices.push_back(index.p1);
                        mesh.indices.push_back(index.p2);
                        mesh.indices.push_back(index.p3);
                    }

                    mesh.CalculateTangents();
                    mesh.CalculateAABB();
                    GLRenderer::sUploadMeshBuffers(mesh);

                    m_ActiveEntity = entity;
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Light")) {
                if (ImGui::MenuItem("Point Light")) {
                    auto entity = scene.CreateSpatialEntity("Point Light");
                    scene.emplace<PointLight>(entity);
                    m_ActiveEntity = entity;
                }

                if (ImGui::MenuItem("Directional Light")) {
                    auto entity = scene.CreateSpatialEntity("Directional Light");
                    scene.emplace<DirectionalLight>(entity);
                    m_ActiveEntity = entity;
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x / 2));

        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);

        if (ImGui::Button(ICON_FA_HAMMER)) {}

        ImGui::SameLine();

        const auto physics_state = GetPhysics().GetState();
        ImGui::PushStyleColor(ImGuiCol_Text, GetPhysics().GetStateColor());
        if (ImGui::Button(physics_state == Physics::Stepping ? ICON_FA_PAUSE : ICON_FA_PLAY)) {
            switch (physics_state) {
                case Physics::Idle: {
                    GetPhysics().SaveState();
                    GetPhysics().SetState(Physics::Stepping);
                } break;
                case Physics::Paused: {
                    GetPhysics().SetState(Physics::Stepping);
                } break;
                case Physics::Stepping: {
                    GetPhysics().SetState(Physics::Paused);
                } break;
            }
        }

        ImGui::PopStyleColor();
        ImGui::SameLine();

        const auto current_physics_state = physics_state;
        if (current_physics_state != Physics::Idle)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));

        if (ImGui::Button(ICON_FA_STOP)) {
            if (physics_state != Physics::Idle) {
                GetPhysics().RestoreState();
                GetPhysics().Step(scene, dt); // Step once to trigger the restored state
                GetPhysics().SetState(Physics::Idle);
            }
        }

        if (current_physics_state != Physics::Idle)
            ImGui::PopStyleColor();

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
        ImGui::EndMainMenuBar();
    }
}

} // raekor