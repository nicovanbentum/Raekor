#include "pch.h"
#include "menubarWidget.h"

#include "OS.h"
#include "gltf.h"
#include "scene.h"
#include "timer.h"
#include "assimp.h"
#include "physics.h"
#include "systems.h"
#include "primitives.h"
#include "components.h"
#include "application.h"
#include "IconsFontAwesome5.h"


namespace Raekor {

RTTI_DEFINE_TYPE_NO_FACTORY(MenubarWidget) {}

MenubarWidget::MenubarWidget(Application* inApp) :
	IWidget(inApp, "Menubar")
{
}


void MenubarWidget::Draw(Widgets* inWidgets, float inDeltaTime)
{
	auto& scene = IWidget::GetScene();

	// workaround for not being able to create a modal popup from the main menu bar scope
	// see also https://github.com/ocornut/imgui/issues/331
	ImGuiID import_settings_popup_id = ImHashStr("IMPORT_SETTINGS_POPUP");

	if (ImGui::BeginMainMenuBar())
	{
		ImGui::Text(reinterpret_cast<const char*>( ICON_FA_ADDRESS_BOOK ));

		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New scene"))
			{
				scene.Clear();
				m_Editor->SetActiveEntity(NULL_ENTITY);
			}

			if (ImGui::MenuItem("Open scene.."))
			{
				std::string filepath = OS::sOpenFileDialog("Scene Files (*.scene)\0*.scene\0");

				if (!filepath.empty())
				{
					Timer timer;
					SDL_SetWindowTitle(m_Editor->GetWindow(), std::string("Raekor Editor - " + filepath).c_str());

					scene.OpenFromFile(filepath, IWidget::GetAssets());

					m_Editor->AddRecentScene(filepath);

					m_Editor->SetActiveEntity(NULL_ENTITY);

					m_Editor->LogMessage("[Scene] Open from file took " + std::to_string(Timer::sToMilliseconds(timer.GetElapsedTime())) + " ms.");
				}
			}

			if (ImGui::MenuItem("Save scene..", "CTRL + S"))
			{
				std::string filepath = OS::sSaveFileDialog("Scene File (*.scene)\0", "scene");

				if (!filepath.empty())
				{
					g_ThreadPool.QueueJob([this, filepath]()
					{
						m_Editor->LogMessage("[Editor] Saving scene...");
						GetScene().SaveToFile(filepath, IWidget::GetAssets());
						m_Editor->LogMessage("[Editor] Saved scene to " + fs::relative(filepath).string() + "");
					});
				}
			}

			if (ImGui::MenuItem("Import scene.."))
			{
				std::string filepath = OS::sOpenFileDialog("Scene Files (*.scene)\0*.scene\0");

				if (!filepath.empty())
				{
					Timer timer;

					m_Editor->SetActiveEntity(NULL_ENTITY);

					auto importer = SceneImporter(GetScene(), m_Editor->GetRenderInterface());
					importer.LoadFromFile(filepath, m_Editor->GetAssets());

					/*ImGui::PushOverrideID(import_settings_popup_id);

					ImGui::OpenPopup("Import Settings");

					ImGui::PopID();*/

					m_Editor->LogMessage("[Scene] Import from file took " + std::to_string(Timer::sToMilliseconds(timer.GetElapsedTime())) + " ms.");
				}
			}

			if (!m_Editor->GetSettings().mRecentScenes.empty())
			{
				if (ImGui::BeginMenu("Recent scenes"))
				{
					for (const auto& scene : m_Editor->GetSettings().mRecentScenes)
					{
						const auto& scene_str = scene.string();
					
						if (ImGui::MenuItem(scene_str.c_str()))
						{
							m_Editor->GetScene()->OpenFromFile(scene_str, IWidget::GetAssets());

							m_Editor->AddRecentScene(scene_str);

							SDL_SetWindowTitle(m_Editor->GetWindow(), std::string("Raekor Editor - " + scene_str).c_str());
						}
					}

					ImGui::EndMenu();
				}
			}


			if (ImGui::MenuItem("Save screenshot.."))
			{
				const auto save_path = OS::sSaveFileDialog("Uncompressed PNG (*.png)\0", "png");

				if (!save_path.empty())
				{
					auto& viewport = m_Editor->GetViewport();

					const auto buffer_size = m_Editor->GetRenderInterface()->GetScreenshotBuffer(nullptr);

					if (buffer_size > 0)
					{
						auto pixels = std::vector<unsigned char>(buffer_size);

						m_Editor->GetRenderInterface()->GetScreenshotBuffer(pixels.data());

						stbi_flip_vertically_on_write(true);
						stbi_write_png(save_path.c_str(), viewport.size.x, viewport.size.y, 4, pixels.data(), viewport.size.x * 4);

						m_Editor->LogMessage("[System] Screenshot saved to " + save_path);
					}
					else 
						m_Editor->LogMessage("[System] Unable to save screenshot to " + save_path);
				}
			}

			if (ImGui::MenuItem("Exit", "Escape"))
				m_Editor->Terminate();

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{

			if (ImGui::MenuItem("Delete", "DELETE"))
			{
				if (m_Editor->GetActiveEntity() != NULL_ENTITY)
				{
					IWidget::GetScene().DestroySpatialEntity(m_Editor->GetActiveEntity());
					m_Editor->SetActiveEntity(NULL_ENTITY);
				}
			}

			if (ImGui::MenuItem("Duplicate", "CTRL+D"))
			{

			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			ImGui::PushItemFlag(ImGuiItemFlags_SelectableDontClosePopup, true);

			if (ImGui::MenuItem(GetTitle().c_str(), "", &m_Open));

			for (const auto& widget : *inWidgets)
			{
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

		if (ImGui::BeginMenu("Add"))
		{
			if (ImGui::MenuItem("Empty", "CTRL+E"))
			{
				auto entity = scene.CreateSpatialEntity("Empty");
				m_Editor->SetActiveEntity(entity);
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Material"))
			{
				auto entity = scene.Create();
				scene.Add<Name>(entity).name = "New Material";
				scene.Add<Material>(entity, Material::Default);
				m_Editor->SetActiveEntity(entity);
			}

			if (ImGui::BeginMenu("Shapes"))
			{
				if (ImGui::MenuItem("Sphere"))
				{
					auto entity = scene.CreateSpatialEntity("Sphere");
					auto& mesh = scene.Add<Mesh>(entity);

					if (m_Editor->GetActiveEntity() != NULL_ENTITY && scene.Has<Node>(m_Editor->GetActiveEntity()))
					{
						auto& node = scene.Get<Node>(entity);
						NodeSystem::sAppend(scene, m_Editor->GetActiveEntity(), scene.Get<Node>(m_Editor->GetActiveEntity()), entity, node);
					}

					gGenerateSphere(mesh, 2.5f, 16, 16);
					m_Editor->GetRenderInterface()->UploadMeshBuffers(entity, mesh);
					m_Editor->SetActiveEntity(entity);
				}

				if (ImGui::MenuItem("Plane"))
				{
					auto entity = scene.CreateSpatialEntity("Plane");
					auto& mesh = scene.Add<Mesh>(entity);

					if (m_Editor->GetActiveEntity() != NULL_ENTITY && scene.Has<Node>(m_Editor->GetActiveEntity()))
					{
						auto& node = scene.Get<Node>(entity);
						NodeSystem::sAppend(scene, m_Editor->GetActiveEntity(), scene.Get<Node>(m_Editor->GetActiveEntity()), entity, node);
					}

					for (const auto& v : UnitPlane::vertices)
					{
						mesh.positions.push_back(v.pos);
						mesh.uvs.push_back(v.uv);
						mesh.normals.push_back(v.normal);
					}

					for (const auto& triangle : UnitPlane::indices)
					{
						mesh.indices.push_back(triangle.p1);
						mesh.indices.push_back(triangle.p2);
						mesh.indices.push_back(triangle.p3);
					}

					mesh.CalculateTangents();
					mesh.CalculateAABB();

					m_Editor->GetRenderInterface()->UploadMeshBuffers(entity, mesh);
					m_Editor->SetActiveEntity(entity);
				}

				if (ImGui::MenuItem("Cube"))
				{
					auto entity = scene.CreateSpatialEntity("Cube");
					auto& mesh = scene.Add<Mesh>(entity);

					if (m_Editor->GetActiveEntity() != NULL_ENTITY && scene.Has<Node>(m_Editor->GetActiveEntity()))
					{
						auto& node = scene.Get<Node>(entity);
						NodeSystem::sAppend(scene, m_Editor->GetActiveEntity(), scene.Get<Node>(m_Editor->GetActiveEntity()), entity, node);
					}

					for (const auto& v : UnitCube::vertices)
					{
						mesh.positions.push_back(v.pos);
						mesh.uvs.push_back(v.uv);
						mesh.normals.push_back(v.normal);
					}

					for (const auto& index : UnitCube::indices)
					{
						mesh.indices.push_back(index.p1);
						mesh.indices.push_back(index.p2);
						mesh.indices.push_back(index.p3);
					}

					//mesh.CalculateNormals();
					mesh.CalculateTangents();
					mesh.CalculateAABB();

					m_Editor->GetRenderInterface()->UploadMeshBuffers(entity, mesh);
					m_Editor->SetActiveEntity(entity);
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Light"))
			{
				if (ImGui::MenuItem("Point Light"))
				{
					auto entity = scene.CreateSpatialEntity("Point Light");
					scene.Add<Light>(entity);
					m_Editor->SetActiveEntity(entity);
				}

				if (ImGui::MenuItem("Directional Light"))
				{
					auto entity = scene.CreateSpatialEntity("Directional Light");
					scene.Add<DirectionalLight>(entity);
					scene.Get<Transform>(entity).rotation.x = 0.1;
					m_Editor->SetActiveEntity(entity);
				}

				ImGui::EndMenu();
			}

			ImGui::EndMenu();
		}

		ImGui::PushOverrideID(import_settings_popup_id);

		if (ImGui::BeginPopupModal("Import Settings", 0, ImGuiWindowFlags_AlwaysAutoResize))
		{
			static auto import_meshes = true;
			static auto import_lights = false;
			ImGui::Checkbox("Import Lights", &import_lights);
			ImGui::Checkbox("Import Meshes", &import_meshes);

			if (ImGui::Button("Import"))
				ImGui::CloseCurrentPopup();

			ImGui::SameLine();

			if (ImGui::Button("Cancel"))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}

		ImGui::PopID();

		ImGui::EndMainMenuBar();
	}
}

} // raekor