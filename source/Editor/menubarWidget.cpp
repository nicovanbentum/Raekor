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

MenubarWidget::MenubarWidget(Editor* editor) : 
    IWidget(editor, "Menubar"),
    m_ActiveEntity(GetActiveEntity())
{}



void MenubarWidget::draw(float dt) {
    auto& scene = IWidget::GetScene();

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New scene")) {
                scene.clear();
                m_ActiveEntity = entt::null;
            }

            if (ImGui::MenuItem("Open scene..")) {
                std::string filepath = OS::sOpenFileDialog("Scene Files (*.scene)\0*.scene\0");

                if (!filepath.empty()) {
                    Timer timer;
                    SDL_SetWindowTitle(editor->GetWindow(), std::string(filepath + " - Raekor Renderer").c_str());
                    scene.OpenFromFile(IWidget::GetAssets(), filepath);
                    m_ActiveEntity = entt::null;
                    std::cout << "Open scene time: " << Timer::sToMilliseconds(timer.GetElapsedTime()) << '\n';
                }
            }

            if (ImGui::MenuItem("Save scene..", "CTRL + S")) {
                std::string filepath = OS::sSaveFileDialog("Scene File (*.scene)\0", "scene");

                if (!filepath.empty()) {
                    scene.SaveToFile(IWidget::GetAssets(), filepath);
                }
            }

            if (ImGui::MenuItem("Load model..")) {
                std::string filepath = OS::sOpenFileDialog("Supported Files(*.gltf, *.fbx, *.glb, *.obj)\0*.gltf;*.fbx;*.glb;*.obj\0");
                
                if (!filepath.empty()) {
                    AssimpImporter importer(scene);
                    importer.SetUploadMeshCallbackFunction(GLRenderer::uploadMeshBuffers);
                    importer.SetUploadMaterialCallbackFunction(GLRenderer::uploadMaterialTextures);
                    importer.SetUploadSkeletonCallbackFunction(GLRenderer::UploadSkeletonBuffers);
                    importer.LoadFromFile(IWidget::GetAssets(), filepath);
                    m_ActiveEntity = sInvalidEntity;
                }
            }

            if (ImGui::MenuItem("Load GLTF..")) {
                std::string filepath = OS::sOpenFileDialog("Supported Files(*.gltf, *.glb)\0*.gltf;*.glb\0");

                if (!filepath.empty()) {
                    GltfImporter importer(scene);
                    importer.SetUploadMeshCallbackFunction(GLRenderer::uploadMeshBuffers);
                    importer.SetUploadMaterialCallbackFunction(GLRenderer::uploadMaterialTextures);
                    importer.SetUploadSkeletonCallbackFunction(GLRenderer::UploadSkeletonBuffers);
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
                    auto& viewport = editor->GetViewport();
                    const auto bufferSize = 4 * viewport.size.x * viewport.size.y;
                    
                    auto pixels = std::vector<unsigned char>(bufferSize);
                    glGetTextureImage(IWidget::GetRenderer().tonemap->result, 0, GL_RGBA, GL_UNSIGNED_BYTE, bufferSize * sizeof(unsigned char), pixels.data());

                    stbi_flip_vertically_on_write(true);
                    stbi_write_png(savePath.c_str(), viewport.size.x, viewport.size.y, 4, pixels.data(), viewport.size.x * 4);
                }
            }

            if (ImGui::MenuItem("Exit", "Escape")) {
                editor->m_Running = false;
            }

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

            for (const auto& widget : editor->GetWidgets()) {
                bool is_visible = widget->IsVisible();

                if (ImGui::MenuItem(widget->GetTitle().c_str(), "", &is_visible)) {
                    is_visible ? widget->Show() : widget->Hide();
                }
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

                    for (int i = 0; i <= stackCount; ++i) {
                        stackAngle = PI / 2 - i * stackStep;        // starting from pi/2 to -pi/2
                        xy = radius * cosf(stackAngle);             // r * cos(u)
                        z = radius * sinf(stackAngle);              // r * sin(u)

                        // add (sectorCount+1) vertices per stack
                        // the first and last vertices have same position and normal, but different tex coords
                        for (int j = 0; j <= sectorCount; ++j) {
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
                    for (int i = 0; i < stackCount; ++i) {
                        k1 = i * (sectorCount + 1);     // beginning of current stack
                        k2 = k1 + sectorCount + 1;      // beginning of next stack

                        for (int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
                            // 2 triangles per sector excluding first and last stacks
                            // k1 => k2 => k1+1
                            if (i != 0) {
                                mesh.indices.push_back(k1);
                                mesh.indices.push_back(k2);
                                mesh.indices.push_back(k1 + 1);
                            }

                            // k1+1 => k2 => k2+1
                            if (i != (stackCount - 1)) {
                                mesh.indices.push_back(k1 + 1);
                                mesh.indices.push_back(k2);
                                mesh.indices.push_back(k2 + 1);
                            }
                        }
                    }

                    mesh.CalculateTangents();
                    mesh.CalculateAABB();
                    GLRenderer::uploadMeshBuffers(mesh);
                }

                if (ImGui::MenuItem("Plane")) {
                    auto entity = scene.CreateSpatialEntity("Plane");
                    auto& mesh = scene.emplace<Mesh>(entity);
                    
                    if (m_ActiveEntity != sInvalidEntity) {
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
                    GLRenderer::uploadMeshBuffers(mesh);
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
                    GLRenderer::uploadMeshBuffers(mesh);

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

        ImGui::EndMainMenuBar();
    }
}

} // raekor