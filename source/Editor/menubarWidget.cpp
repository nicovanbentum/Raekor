#include "pch.h"
#include "menubarWidget.h"
#include "Raekor/OS.h"
#include "Raekor/gltf.h"
#include "Raekor/assimp.h"
#include "Raekor/systems.h"
#include "Raekor/scene.h"
#include "Raekor/timer.h"
#include "Raekor/physics.h"
#include "Raekor/application.h"
#include "IconsFontAwesome5.h"


namespace Raekor {

RTTI_CLASS_CPP_NO_FACTORY(MenubarWidget) {}

MenubarWidget::MenubarWidget(Application* inApp) : 
    IWidget(inApp, "Menubar")
{}


void MenubarWidget::Draw(Widgets* inWidgets, float inDeltaTime) {
    auto& scene = IWidget::GetScene();

    if (ImGui::BeginMainMenuBar()) {
        ImGui::Text(reinterpret_cast<const char*>(ICON_FA_ADDRESS_BOOK));

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New scene")) {
                scene.clear();
                m_Editor->SetActiveEntity(sInvalidEntity);
            }

            if (ImGui::MenuItem("Open scene..")) {
                std::string filepath = OS::sOpenFileDialog("Scene Files (*.scene)\0*.scene\0");

                if (!filepath.empty()) {
                    Timer timer;
                    SDL_SetWindowTitle(m_Editor->GetWindow(), std::string(filepath + " - Raekor Renderer").c_str());
                    scene.OpenFromFile(IWidget::GetAssets(), filepath);
                    m_Editor->SetActiveEntity(sInvalidEntity);

                    m_Editor->LogMessage("[Scene] Open from file took " + std::to_string(Timer::sToMilliseconds(timer.GetElapsedTime())) + " ms.");
                }
            }

            if (ImGui::MenuItem("Save scene..", "CTRL + S")) {
                std::string filepath = OS::sSaveFileDialog("Scene File (*.scene)\0", "scene");

                if (!filepath.empty()) {
                    g_ThreadPool.QueueJob([this, filepath]() {
                        m_Editor->LogMessage("Saving scene...");
                        GetScene().SaveToFile(IWidget::GetAssets(), filepath);
                        m_Editor->LogMessage("[Scene] Saved to " + FileSystem::relative(filepath).string() + "");
                    });
                }
            }

            if (ImGui::MenuItem("Serialize as JSON..", "CTRL + S")) {
                auto folder = Path(OS::sSelectFolderDialog());

                Timer timer;
                if (!folder.empty()) {
                    for (const auto& [entity, name, mesh] : scene.view<Name, Mesh>().each()) {
                        // for the love of god C++ overlords, let me capture structured bindings
                        g_ThreadPool.QueueJob([&, name = &name, mesh = &mesh]() {
                            auto ofs = std::ofstream((folder / "sponza").replace_extension(".meshes"));
                            cereal::JSONOutputArchive archive(ofs);
                            archive(*mesh);
                        });
                    }

                    for (const auto& [entity, name, material] : scene.view<Name, Material>().each()) {
                        // for the love of god C++ overlords, let me capture structured bindings
                        g_ThreadPool.QueueJob([&, name = &name, material = &material]() {
                            auto ofs = std::ofstream((folder / name->name).replace_extension(".material"));
                            cereal::JSONOutputArchive archive(ofs);
                            archive(*material);
                        });
                    }
                    g_ThreadPool.WaitForJobs();
                }

                m_Editor->LogMessage("[Scene] Serialize as JSON took " + std::to_string(Timer::sToMilliseconds(timer.GetElapsedTime())) + " ms.");
            }

            if (ImGui::MenuItem("Load model..")) {
                const auto filepath = OS::sOpenFileDialog("Supported Files(*.gltf, *.fbx, *.glb, *.obj, *.blend)\0*.gltf;*.fbx;*.glb;*.obj;*.blend\0");
                
                if (!filepath.empty()) {
                    auto importer = AssimpImporter(scene, m_Editor->GetRenderer());
                    importer.LoadFromFile(GetAssets(), filepath);
                    m_Editor->SetActiveEntity(sInvalidEntity);
                }
            }

            if (ImGui::MenuItem("Load GLTF..")) {
                const auto filepath = OS::sOpenFileDialog("Supported Files(*.gltf, *.glb)\0*.gltf;*.glb\0");

                if (!filepath.empty()) {
                    auto importer = GltfImporter(scene, m_Editor->GetRenderer());
                    importer.LoadFromFile(GetAssets(), filepath);

                    m_Editor->SetActiveEntity(sInvalidEntity);
                    m_Editor->LogMessage("[Scene] Loaded " + Path(filepath).filename().string());
                }
            }

            if (ImGui::MenuItem("Compile script..")) {
                const auto filepath = OS::sOpenFileDialog("DLL Files (*.dll)\0*.dll\0");
                if (!filepath.empty()) {
                    //g_ThreadPool.QueueJob([filepath]() {
                        ScriptAsset::sConvert(filepath);
                   // });
                }
            }

            if (ImGui::MenuItem("Save screenshot..")) {
                const auto save_path = OS::sSaveFileDialog("Uncompressed PNG (*.png)\0", "png");

                if (!save_path.empty()) {
                    auto& viewport = m_Editor->GetViewport();

                    const auto buffer_size = m_Editor->GetRenderer()->GetScreenshotBuffer(nullptr);

                    auto pixels = std::vector<unsigned char>(buffer_size);

                    m_Editor->GetRenderer()->GetScreenshotBuffer(pixels.data());

                    stbi_flip_vertically_on_write(true);
                    stbi_write_png(save_path.c_str(), viewport.size.x, viewport.size.y, 4, pixels.data(), viewport.size.x * 4);

                    m_Editor->LogMessage("[System] Screenshot saved to " + save_path);
                }
            }

            if (ImGui::MenuItem("Exit", "Escape"))
                m_Editor->Terminate();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {

            if (ImGui::MenuItem("Delete", "DEL")) {
                if (m_Editor->GetActiveEntity() != sInvalidEntity) {
                    IWidget::GetScene().DestroySpatialEntity(m_Editor->GetActiveEntity());
                    m_Editor->SetActiveEntity(sInvalidEntity);
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::PushItemFlag(ImGuiItemFlags_SelectableDontClosePopup, true);

           for (const auto& widget : *inWidgets) {
               // skip the menu bar widget itself
                if (widget.get() == this)
                    continue;

                auto is_visible = widget->IsOpen();
                
                if (ImGui::MenuItem(std::string(widget->GetTitle() + "Window").c_str(), "", &is_visible))
                    is_visible ? widget->Show() : widget->Hide();
            }

            ImGui::PopItemFlag();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Add")) {
            if (ImGui::MenuItem("Empty", "CTRL+E")) {
                auto entity = scene.CreateSpatialEntity("Empty");
                m_Editor->SetActiveEntity(entity);
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Material")) {
                auto entity = scene.create();
                scene.emplace<Name>(entity).name =  "New Material";
                scene.emplace<Material>(entity);
                m_Editor->SetActiveEntity(entity);
            }

            if (ImGui::BeginMenu("Shapes")) {
                if (ImGui::MenuItem("Sphere")) {
                    auto entity = scene.CreateSpatialEntity("Sphere");
                    auto& mesh = scene.emplace<Mesh>(entity);

                    if (m_Editor->GetActiveEntity() != sInvalidEntity) {
                        auto& node = scene.get<Node>(entity);
                        NodeSystem::sAppend(scene, m_Editor->GetActiveEntity(), scene.get<Node>(m_Editor->GetActiveEntity()), entity, node);
                    }

                    gGenerateSphere(mesh, 2.5f, 16, 16);
                    m_Editor->GetRenderer()->UploadMeshBuffers(mesh);
                }

                if (ImGui::MenuItem("Plane")) {
                    auto entity = scene.CreateSpatialEntity("Plane");
                    auto& mesh = scene.emplace<Mesh>(entity);
                    
                    if (m_Editor->GetActiveEntity() != sInvalidEntity && scene.any_of<Node>(m_Editor->GetActiveEntity())) {
                        auto& node = scene.get<Node>(entity);
                        NodeSystem::sAppend(scene, m_Editor->GetActiveEntity(), scene.get<Node>(m_Editor->GetActiveEntity()), entity, node);
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

                    m_Editor->GetRenderer()->UploadMeshBuffers(mesh);
                }

                if (ImGui::MenuItem("Cube")) {
                    auto entity = scene.CreateSpatialEntity("Cube");
                    auto& mesh = scene.emplace<Mesh>(entity);

                    if (m_Editor->GetActiveEntity() != sInvalidEntity && scene.all_of<Node>(m_Editor->GetActiveEntity())) {
                        auto& node = scene.get<Node>(entity);
                        NodeSystem::sAppend(scene, m_Editor->GetActiveEntity(), scene.get<Node>(m_Editor->GetActiveEntity()), entity, node);
                    }

                    for (const auto& v : UnitCube::vertices) {
                        mesh.positions.push_back(v.pos);
                        mesh.uvs.push_back(v.uv);
                        mesh.normals.push_back(v.normal);
                    }

                    for (const auto& index : UnitCube::indices) {
                        mesh.indices.push_back(index.p1);
                        mesh.indices.push_back(index.p2);
                        mesh.indices.push_back(index.p3);
                    }

                    //mesh.CalculateNormals();
                    mesh.CalculateTangents();
                    mesh.CalculateAABB();
                    
                    m_Editor->GetRenderer()->UploadMeshBuffers(mesh);
                    m_Editor->SetActiveEntity(entity);
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Light")) {
                if (ImGui::MenuItem("Point Light")) {
                    auto entity = scene.CreateSpatialEntity("Point Light");
                    scene.emplace<PointLight>(entity);
                    m_Editor->SetActiveEntity(entity);
                }

                if (ImGui::MenuItem("Directional Light")) {
                    auto entity = scene.CreateSpatialEntity("Directional Light");
                    scene.emplace<DirectionalLight>(entity);
                    m_Editor->SetActiveEntity(entity);
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x / 2));

        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);

        if (ImGui::Button((const char*)ICON_FA_HAMMER)) {}

        ImGui::SameLine();

        const auto physics_state = GetPhysics().GetState();
        ImGui::PushStyleColor(ImGuiCol_Text, GetPhysics().GetStateColor());
        if (ImGui::Button(physics_state == Physics::Stepping ? (const char*)ICON_FA_PAUSE : (const char*)ICON_FA_PLAY)) {
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

        if (ImGui::Button((const char*)ICON_FA_STOP)) {
            if (physics_state != Physics::Idle) {
                GetPhysics().RestoreState();
                GetPhysics().Step(scene, inDeltaTime); // Step once to trigger the restored state
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