#include "pch.h"
#include "menubarWidget.h"
#include "editor.h"
#include "../platform/OS.h"
#include "assimp.h"
#include "systems.h"
#include "mesh.h"
#include "scene.h"

namespace Raekor {

MenubarWidget::MenubarWidget(Editor* editor) : 
    IWidget(editor, "Menubar"),
    active(editor->active)
{}



void MenubarWidget::draw() {
    auto& scene = IWidget::scene();

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New scene")) {
                scene.clear();
                editor->active = entt::null;
            }

            if (ImGui::MenuItem("Open scene..")) {
                std::string filepath = OS::openFileDialog("Scene Files (*.scene)\0*.scene\0");

                if (!filepath.empty()) {
                    SDL_SetWindowTitle(editor->getWindow(), std::string(filepath + " - Raekor Renderer").c_str());
                    scene.openFromFile(IWidget::async(), IWidget::assets(), filepath);
                    editor->active = entt::null;
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
                    AssimpImporter importer(scene);
                    importer.LoadFromFile(IWidget::async(), IWidget::assets(), filepath);
                    active = entt::null;
                }
            }

            if (ImGui::MenuItem("Compile script..")) {
                std::string filepath = OS::openFileDialog("C++ Files (*.cpp)\0*.cpp\0");
                if (!filepath.empty()) {
                    ScriptAsset::convert(filepath);
                }
            }

            if (ImGui::MenuItem("Save screenshot..")) {
                std::string savePath = OS::saveFileDialog("Uncompressed PNG (*.png)\0", "png");

                if (!savePath.empty()) {
                    auto& viewport = editor->getViewport();
                    const auto bufferSize = 4 * viewport.size.x * viewport.size.y;
                    
                    auto pixels = std::vector<unsigned char>(bufferSize);
                    glGetTextureImage(IWidget::renderer().tonemap->result, 0, GL_RGBA, GL_UNSIGNED_BYTE, bufferSize * sizeof(unsigned char), pixels.data());

                    stbi_flip_vertically_on_write(true);
                    stbi_write_png(savePath.c_str(), viewport.size.x, viewport.size.y, 4, pixels.data(), viewport.size.x * 4);
                }
            }

            if (ImGui::MenuItem("Exit", "Escape")) {
                editor->running = false;
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {

            if (ImGui::MenuItem("Delete", "DEL")) {
                if (active != entt::null) {
                    IWidget::scene().destroyObject(active);
                    active = entt::null;
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::PushItemFlag(ImGuiItemFlags_SelectableDontClosePopup, true);

            for (const auto& widget : editor->getWidgets()) {
                if (ImGui::MenuItem(widget->getTitle().c_str(), "", &widget->isVisible())) {}
            }

            ImGui::PopItemFlag();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Add")) {
            if (ImGui::MenuItem("Empty", "CTRL+E")) {
                auto entity = scene.createObject("Empty");
                active = entity;
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Material")) {
                auto entity = scene.create();
                scene.emplace<Name>(entity, "New Material");
                scene.emplace<Material>(entity);
                active = entity;
            }

            if (ImGui::BeginMenu("Shapes")) {
                if (ImGui::MenuItem("Sphere")) {
                    auto entity = scene.createObject("Sphere");
                    auto& mesh = scene.emplace<Mesh>(entity);

                    if (active != entt::null) {
                        auto& node = scene.get<Node>(entity);
                        NodeSystem::append(scene, scene.get<Node>(active), node);
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

                    mesh.generateTangents();
                    mesh.uploadVertices();
                    mesh.uploadIndices();
                    mesh.generateAABB();
                }

                if (ImGui::MenuItem("Plane")) {
                    auto entity = scene.createObject("Plane");
                    auto& mesh = scene.emplace<Mesh>(entity);
                    
                    if (active != entt::null) {
                        auto& node = scene.get<Node>(entity);
                        NodeSystem::append(scene, scene.get<Node>(active), node);
                    }
                    
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

                if (ImGui::MenuItem("Cube")) {
                    auto entity = scene.createObject("Cube");
                    auto& mesh = scene.emplace<Mesh>(entity);

                    if (active != entt::null) {
                        auto& node = scene.get<Node>(entity);
                        NodeSystem::append(scene, scene.get<Node>(active), node);
                    }

                    for (const auto& v : unitCubeVertices) {
                        mesh.positions.push_back(v.pos);
                        mesh.uvs.push_back(v.uv);
                        mesh.normals.push_back(v.pos);
                    }

                    for (const auto& index : cubeIndices) {
                        mesh.indices.push_back(index.p1);
                        mesh.indices.push_back(index.p2);
                        mesh.indices.push_back(index.p3);
                    }

                    mesh.generateTangents();
                    mesh.uploadVertices();
                    mesh.uploadIndices();
                    mesh.generateAABB();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Light")) {
                if (ImGui::MenuItem("Point Light")) {
                    auto entity = scene.createObject("Point Light");
                    scene.emplace<PointLight>(entity);
                    active = entity;
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

} // raekor