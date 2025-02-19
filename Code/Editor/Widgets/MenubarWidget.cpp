#include "pch.h"
#include "MenubarWidget.h"

#include "OS.h"
#include "OBJ.h"
#include "Fbx.h"
#include "Gltf.h"
#include "Scene.h"
#include "Input.h"
#include "Timer.h"
#include "Editor.h"
#include "Assimp.h"
#include "Physics.h"
#include "Compiler.h"
#include "Primitives.h"
#include "Components.h"
#include "Application.h"
#include "IconsFontAwesome5.h"


namespace RK {

RTTI_DEFINE_TYPE_NO_FACTORY(MenubarWidget) {}

MenubarWidget::MenubarWidget(Editor* inEditor) :
	IWidget(inEditor, "Menubar")
{
}

void MenubarWidget::Draw(Widgets* inWidgets, float inDeltaTime)
{
	Scene& scene = IWidget::GetScene();

	// workaround for not being able to create a modal popup from the main menu bar scope
	// see also https://github.com/ocornut/imgui/issues/331
	ImGuiID import_settings_popup_id = ImHashStr("IMPORT_SETTINGS_POPUP");

	if (ImGui::BeginMainMenuBar())
	{
		ImGui::Text(reinterpret_cast<const char*>( ICON_FA_ADDRESS_BOOK ));

		if (ImGui::BeginPopupContextItem(reinterpret_cast<const char*>( ICON_FA_ADDRESS_BOOK )))
		{
			if (ImGui::MenuItem("Hide"))
				Hide();

			ImGui::EndPopup();
		}

		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New scene"))
			{
				scene.Clear();
				m_Editor->SetActiveEntity(Entity::Null);
			}

			if (ImGui::MenuItem("Open scene.."))
			{
				String filepath = OS::sOpenFileDialog("Scene Files (*.scene)\0*.scene\0");

				if (!filepath.empty())
				{
					Timer timer;
					SDL_SetWindowTitle(m_Editor->GetWindow(), std::string("RK Editor - " + filepath).c_str());

					scene.OpenFromFileAsync(filepath, IWidget::GetAssets(), m_Editor);

					m_Editor->AddRecentScene(filepath);

					m_Editor->SetActiveEntity(Entity::Null);

					m_Editor->LogMessage("[Scene] Open from file took " + std::to_string(Timer::sToMilliseconds(timer.GetElapsedTime())) + " ms.");
				}
			}

			if (ImGui::MenuItem("Save scene..", "CTRL + S"))
			{
				String filepath = OS::sSaveFileDialog("Scene File (*.scene)\0", "scene");

				if (!filepath.empty())
				{
					g_ThreadPool.QueueJob([this, filepath]()
					{
						m_Editor->LogMessage("[Editor] Saving scene...");
						GetScene().SaveToFile(filepath, IWidget::GetAssets());
						m_Editor->LogMessage("[Editor] Saved scene to " + fs::relative(filepath).string() + "");
					});
				}

				m_Editor->AddRecentScene(filepath);
			}

			if (ImGui::MenuItem("Import scene.."))
			{
				std::string filepath = OS::sOpenFileDialog("Scene Files(*.scene, *.gltf, *.glb, *.fbx, *.obj)\0*.scene;*.gltf;*.glb;*.fbx;*.obj\0");

				if (!filepath.empty())
				{
					Timer timer;

					m_Editor->SetActiveEntity(Entity::Null);

					const Path extension = fs::path(filepath).extension();

					Importer* importer = nullptr;

					if (extension == ".gltf" || extension == ".glb")
						importer = new GltfImporter(GetScene(), &GetRenderInterface());
					else if (extension == ".fbx")
						importer = new FBXImporter(GetScene(), &GetRenderInterface());
					else if (extension == ".obj")
						importer = new OBJImporter(GetScene(), &GetRenderInterface());
					else if (extension == ".scene")
						importer = new SceneImporter(GetScene(), &GetRenderInterface());

					if (importer)
					{
						importer->LoadFromFile(filepath, &GetAssets());

						m_Editor->LogMessage(std::format("[Scene] Import from file took {:.0f} ms", Timer::sToMilliseconds(timer.GetElapsedTime())));
					}
				}
			}

			if (!m_Editor->GetConfigSettings().mRecentScenes.empty())
			{
				if (ImGui::BeginMenu("Recent scenes"))
				{
					for (const Path& scene_path : m_Editor->GetConfigSettings().mRecentScenes)
					{
						const String& scene_str = scene_path.string();

						if (scene_str.empty())
							continue;
					
						if (ImGui::MenuItem(scene_str.c_str()))
						{
							if (fs::exists(scene_str))
							{
								m_Editor->GetScene()->OpenFromFile(scene_str, IWidget::GetAssets(), m_Editor);

								m_Editor->AddRecentScene(scene_str);

								SDL_SetWindowTitle(m_Editor->GetWindow(), String("RK Editor - " + scene_str).c_str());
							}
						}
					}

					ImGui::EndMenu();
				}
			}


			if (ImGui::MenuItem("Save screenshot.."))
			{
				const String save_path = OS::sSaveFileDialog("Uncompressed PNG (*.png)\0", "png");

				if (!save_path.empty())
				{
					Viewport& viewport = m_Editor->GetViewport();

					const uint32_t buffer_size = m_Editor->GetRenderInterface()->GetScreenshotBuffer(nullptr);

					if (buffer_size > 0)
					{
						Array<unsigned char> pixel_data(buffer_size);

						m_Editor->GetRenderInterface()->GetScreenshotBuffer(pixel_data.data());

						stbi_flip_vertically_on_write(true);
						stbi_write_png(save_path.c_str(), viewport.size.x, viewport.size.y, 4, pixel_data.data(), viewport.size.x * 4);

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

			if (ImGui::MenuItem("Undo", "CTRL+Z", false, m_Editor->GetUndo()->HasUndo()))
			{
				m_Editor->GetUndo()->Undo();
			}

			if (ImGui::MenuItem("Redo", "CTRL+Y", false, m_Editor->GetUndo()->HasRedo()))
			{
				m_Editor->GetUndo()->Redo();
			}

			if (ImGui::MenuItem("Delete", "DELETE"))
			{
				if (m_Editor->GetActiveEntity() != Entity::Null)
				{
					IWidget::GetScene().DestroySpatialEntity(m_Editor->GetActiveEntity());
					m_Editor->SetActiveEntity(Entity::Null);
				}
			}

			if (ImGui::MenuItem("Duplicate", "CTRL+D"))
			{
				if (GetActiveEntity() != Entity::Null && GetActiveEntity() != GetScene().GetRootEntity())
					SetActiveEntity(GetScene().Clone(GetActiveEntity()));
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window"))
		{
			ImGui::PushItemFlag(ImGuiItemFlags_None, true);

			constexpr const char* subtext = "CTRL+Click";

			if (ImGui::MenuItem(GetTitle().c_str(), subtext, &m_Open));

			for (const auto& widget : *inWidgets)
			{
				// skip the menu bar widget itself
				if (widget.get() == this)
					continue;

				bool is_visible = widget->IsOpen();

				if (ImGui::MenuItem(std::string(widget->GetTitle() + "Window").c_str(), subtext, &is_visible))
				{
					if (SDL_GetModState() & SDL_KMOD_LCTRL)
					{
						widget->Show();

						for (const auto& other_widget : *inWidgets)
						{
							if (widget.get() == other_widget.get())
								continue;

							other_widget->Hide();
						}
					}
					else
						is_visible ? widget->Show() : widget->Hide();

				}
			}

			ImGui::PopItemFlag();

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Add"))
		{
			if (ImGui::MenuItem("Empty", "CTRL+E"))
			{
				Entity entity = scene.CreateSpatialEntity("Empty");
				m_Editor->SetActiveEntity(entity);
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Camera"))
			{
				Entity entity = scene.CreateSpatialEntity("New Camera");
				Camera& camera = scene.Add<Camera>(entity);
				camera.SetFar(1000.0f);
				m_Editor->SetActiveEntity(entity);
			}

			if (ImGui::MenuItem("Material"))
			{
				Entity entity = scene.Create();
				scene.Add<Name>(entity).name = "Material";
				scene.Add<Material>(entity, Material::Default);
				m_Editor->SetActiveEntity(entity);
			}

			if (ImGui::MenuItem("DDGI Settings"))
			{
				Entity entity = scene.CreateSpatialEntity("DDGI Settings");
				DDGISceneSettings& ddgi_settings = scene.Add<DDGISceneSettings>(entity);
				
				ddgi_settings.FitToScene(scene, scene.Get<Transform>(entity));
				
				m_Editor->SetActiveEntity(entity);
			}

			if (ImGui::BeginMenu("Shapes"))
			{
				if (ImGui::MenuItem("Sphere"))
				{
					Entity entity = scene.CreateSpatialEntity("Sphere");
					
					Mesh& mesh = scene.Add<Mesh>(entity);
					Mesh::CreateSphere(mesh, 0.5f, 32, 32);

					m_Editor->GetRenderInterface()->UploadMeshBuffers(entity, mesh);
					m_Editor->SetActiveEntity(entity);
				}

				if (ImGui::MenuItem("Plane"))
				{
					Entity entity = scene.CreateSpatialEntity("Plane");
					
					Mesh& mesh = scene.Add<Mesh>(entity);
					Mesh::CreatePlane(mesh, 1.0f);

					m_Editor->GetRenderInterface()->UploadMeshBuffers(entity, mesh);
					m_Editor->SetActiveEntity(entity);
				}

				if (ImGui::MenuItem("Cube"))
				{
					Entity entity = scene.CreateSpatialEntity("Cube");
					
					Mesh& mesh = scene.Add<Mesh>(entity);
					Mesh::CreateCube(mesh, 1.0f);

					m_Editor->GetRenderInterface()->UploadMeshBuffers(entity, mesh);
					m_Editor->SetActiveEntity(entity);
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Light"))
			{
				if (ImGui::MenuItem("Spot Light"))
				{
					Entity entity = scene.CreateSpatialEntity("Spot Light");
					scene.Add<Light>(entity).type = LIGHT_TYPE_SPOT;
					m_Editor->SetActiveEntity(entity);
				}

				if (ImGui::MenuItem("Point Light"))
				{
					Entity entity = scene.CreateSpatialEntity("Point Light");
					scene.Add<Light>(entity).type = LIGHT_TYPE_POINT;
					m_Editor->SetActiveEntity(entity);
				}

				if (ImGui::MenuItem("Directional Light"))
				{
					Entity entity = scene.CreateSpatialEntity("Directional Light");
					scene.Add<DirectionalLight>(entity);
					scene.Get<Transform>(entity).rotation.x = 0.1;
					m_Editor->SetActiveEntity(entity);
				}

				ImGui::EndMenu();
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Tools"))
		{
			if (ImGui::MenuItem("Launch Shader Editor"))
			{
				const String exe = OS::sGetExecutablePath().string();
				ShellExecute(0, 0, exe.c_str(), "-shader_editor", 0, SW_SHOW);
			}

			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Launch Shader Editor process to the system tray.");

			if (ImGui::MenuItem("Launch Asset Compiler"))
			{
				const String exe = OS::sGetExecutablePath().string();
				ShellExecute(0, 0, exe.c_str(), "-asset_compiler", 0, SW_SHOW);
			}

			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Launch Asset Compiler process to the system tray.");

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			ImGui::EndMenu();
		}

		ImGui::PushOverrideID(import_settings_popup_id);

		if (ImGui::BeginPopupModal("Import Settings", 0, ImGuiWindowFlags_AlwaysAutoResize))
		{
			static bool import_meshes = true;
			static bool import_lights = false;
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