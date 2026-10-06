#include "pch.h"
#include "MenubarWidget.h"

#include "OS.h"
#include "Scene.h"
#include "Input.h"
#include "Timer.h"
#include "Editor.h"
#include "Physics.h"
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
	if (ImGui::BeginMainMenuBar())
	{
		DrawFileMenu();
		DrawEditMenu();
		DrawViewMenu(inWidgets);
		DrawCreateMenu();
		DrawPlayMenu();
		DrawToolsMenu();
		DrawHelpMenu();

		ImGui::EndMainMenuBar();
	}

	DrawShortcutsWindow();
	DrawAboutWindow();
}


void MenubarWidget::DrawFileMenu()
{
	if (!ImGui::BeginMenu("File"))
		return;

	const bool is_editing = m_Editor->GetGameState() != GAME_RUNNING;
	const bool is_loading = m_Editor->GetSceneTask() != nullptr;

	ImGui::BeginDisabled(!is_editing || is_loading);

	if (ImGui::MenuItem((const char*)ICON_FA_FILE "  New Scene", "Ctrl+N"))
		m_Editor->RunAfterUnsavedChangesCheck([this]() { m_Editor->NewScene(); });

	if (ImGui::MenuItem((const char*)ICON_FA_FOLDER_OPEN "  Open Scene..", "Ctrl+O"))
		m_Editor->RunAfterUnsavedChangesCheck([this]() { m_Editor->OpenSceneDialog(); });

	const Array<Path>& recent_scenes = m_Editor->GetConfigSettings().mRecentScenes;

	if (ImGui::BeginMenu("Open Recent", !recent_scenes.empty()))
	{
		for (const Path& scene_path : recent_scenes)
		{
			if (scene_path.empty())
				continue;

			const bool exists = fs::exists(scene_path);
			const String label = std::format("{}##{}", scene_path.filename().string(), scene_path.string());

			if (ImGui::MenuItem(label.c_str(), nullptr, false, exists))
			{
				const Path path = scene_path;
				m_Editor->RunAfterUnsavedChangesCheck([this, path]() { m_Editor->OpenScene(path); });
			}

			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip(exists ? "%s" : "%s (missing)", scene_path.string().c_str());
		}

		ImGui::EndMenu();
	}

	ImGui::Separator();

	if (ImGui::MenuItem((const char*)ICON_FA_SAVE "  Save", "Ctrl+S", false, !m_Editor->IsSaving()))
		m_Editor->SaveScene();

	if (ImGui::MenuItem("      Save As..", "Ctrl+Shift+S", false, !m_Editor->IsSaving()))
		m_Editor->SaveSceneAs();

	ImGui::Separator();

	if (ImGui::MenuItem((const char*)ICON_FA_FILE_IMPORT "  Import..", "Ctrl+I"))
		m_Editor->ImportSceneDialog();

	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("Add the contents of a .scene, .gltf, .glb, .fbx or .obj file to the open scene.");

	ImGui::EndDisabled();

	ImGui::Separator();

	if (ImGui::MenuItem((const char*)ICON_FA_IMAGE "  Save Screenshot.."))
		SaveScreenshot();

	ImGui::Separator();

	if (ImGui::MenuItem("      Exit", "Alt+F4"))
	{
		m_Editor->RunAfterUnsavedChangesCheck([this]() { m_Editor->Terminate(); });
	}

	ImGui::EndMenu();
}


void MenubarWidget::DrawEditMenu()
{
	if (!ImGui::BeginMenu("Edit"))
		return;

	const bool is_editing = m_Editor->GetGameState() != GAME_RUNNING;
	const Entity active_entity = GetActiveEntity();
	const bool has_selection = active_entity != Entity::Null && active_entity != GetScene().GetRootEntity();

	ImGui::BeginDisabled(!is_editing);

	if (ImGui::MenuItem((const char*)ICON_FA_UNDO "  Undo", "Ctrl+Z", false, m_Editor->GetUndo()->HasUndo()))
	{
		m_Editor->GetUndo()->Undo();
		m_Editor->SetViewportChanged(true);
		m_Editor->MarkSceneChanged();
	}

	if (ImGui::MenuItem((const char*)ICON_FA_REDO "  Redo", "Ctrl+Y", false, m_Editor->GetUndo()->HasRedo()))
	{
		m_Editor->GetUndo()->Redo();
		m_Editor->SetViewportChanged(true);
		m_Editor->MarkSceneChanged();
	}

	ImGui::Separator();

	if (ImGui::MenuItem((const char*)ICON_FA_CLONE "  Duplicate", "Ctrl+D", false, has_selection))
		m_Editor->DuplicateSelection();

	if (ImGui::MenuItem((const char*)ICON_FA_TRASH "  Delete", "Delete", false, has_selection || m_Editor->GetMultiSelect().Size > 0))
		m_Editor->DeleteSelection();

	ImGui::Separator();

	if (ImGui::MenuItem((const char*)ICON_FA_TIMES "  Deselect", "Escape", false, has_selection))
		SetActiveEntity(Entity::Null);

	ImGui::EndDisabled();

	ImGui::EndMenu();
}


void MenubarWidget::DrawViewMenu(Widgets* inWidgets)
{
	if (!ImGui::BeginMenu("View"))
		return;

	ImGui::SeparatorText("Windows");

	for (const auto& widget : *inWidgets)
	{
		if (widget.get() == this)
			continue;

		bool is_open = widget->IsOpen();

		if (ImGui::MenuItem(widget->GetTitle().c_str(), nullptr, &is_open))
			is_open ? widget->Show() : widget->Hide();
	}

	ImGui::Separator();

	if (ImGui::MenuItem("Show All Windows"))
	{
		for (const auto& widget : *inWidgets)
			widget->Show();
	}

	if (ImGui::MenuItem("Reset Layout"))
	{
		for (const auto& widget : *inWidgets)
			widget->Show();

		m_Editor->ResetLayout();
	}

	if (ImGui::MenuItem("Maximize Viewport", "F1"))
	{
		SDL_Event event = {};
		event.type = SDL_EVENT_KEY_DOWN;
		event.key.key = SDLK_F1;
		SDL_PushEvent(&event);
	}

	ImGui::EndMenu();
}


void MenubarWidget::SelectCreatedEntity(Entity inEntity)
{
	m_Editor->SetActiveEntity(inEntity);
	m_Editor->SetViewportChanged(true);
	m_Editor->MarkSceneChanged();
}


void MenubarWidget::DrawCreateMenu()
{
	if (!ImGui::BeginMenu("Create"))
		return;

	Scene& scene = GetScene();

	ImGui::BeginDisabled(m_Editor->GetGameState() == GAME_RUNNING);

	if (ImGui::MenuItem((const char*)ICON_FA_CUBE "  Empty Entity"))
		SelectCreatedEntity(scene.CreateSpatialEntity("Empty"));

	ImGui::Separator();

	if (ImGui::BeginMenu((const char*)ICON_FA_CUBES "  Shapes"))
	{
		auto CreateShape = [&](const char* inName, auto inCreateFunction)
		{
			const Entity entity = scene.CreateSpatialEntity(inName);

			Mesh& mesh = scene.Add<Mesh>(entity);
			inCreateFunction(mesh);

			GetRenderInterface().UploadMeshBuffers(entity, mesh);
			SelectCreatedEntity(entity);
		};

		if (ImGui::MenuItem("Cube"))
			CreateShape("Cube", [](Mesh& ioMesh) { Mesh::CreateCube(ioMesh, 1.0f); });

		if (ImGui::MenuItem("Sphere"))
			CreateShape("Sphere", [](Mesh& ioMesh) { Mesh::CreateSphere(ioMesh, 0.5f, 32, 32); });

		if (ImGui::MenuItem("Plane"))
			CreateShape("Plane", [](Mesh& ioMesh) { Mesh::CreatePlane(ioMesh, 1.0f); });

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu((const char*)ICON_FA_LIGHTBULB "  Lights"))
	{
		if (ImGui::MenuItem("Point Light"))
		{
			const Entity entity = scene.CreateSpatialEntity("Point Light");
			scene.Add<Light>(entity).type = LIGHT_TYPE_POINT;
			SelectCreatedEntity(entity);
		}

		if (ImGui::MenuItem("Spot Light"))
		{
			const Entity entity = scene.CreateSpatialEntity("Spot Light");
			scene.Add<Light>(entity).type = LIGHT_TYPE_SPOT;
			SelectCreatedEntity(entity);
		}

		ImGui::BeginDisabled(scene.Count<DirectionalLight>() > 0);

		if (ImGui::MenuItem("Directional Light"))
		{
			const Entity entity = scene.CreateSpatialEntity("Directional Light");
			scene.Add<DirectionalLight>(entity);
			scene.Get<Transform>(entity).rotation.x = 0.1;
			SelectCreatedEntity(entity);
		}

		ImGui::EndDisabled();

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && scene.Count<DirectionalLight>() > 0)
			ImGui::SetTooltip("The scene already has a directional light.");

		ImGui::EndMenu();
	}

	if (ImGui::MenuItem((const char*)ICON_FA_CAMERA "  Camera"))
	{
		const Entity entity = scene.CreateSpatialEntity("Camera");
		scene.Add<Camera>(entity).SetFar(1000.0f);
		SelectCreatedEntity(entity);
	}

	ImGui::Separator();

	if (ImGui::MenuItem((const char*)ICON_FA_PALETTE "  Material"))
	{
		const Entity entity = scene.Create();
		scene.Add<Name>(entity).name = "Material";
		scene.Add<Material>(entity, Material::Default);
		SelectCreatedEntity(entity);
	}

	ImGui::BeginDisabled(scene.Count<DDGISceneSettings>() > 0);

	if (ImGui::MenuItem((const char*)ICON_FA_GLOBE "  Global Illumination Volume"))
	{
		const Entity entity = scene.CreateSpatialEntity("DDGI Settings");
		DDGISceneSettings& ddgi_settings = scene.Add<DDGISceneSettings>(entity);
		ddgi_settings.FitToScene(scene, scene.Get<Transform>(entity));
		SelectCreatedEntity(entity);
	}

	ImGui::EndDisabled();

	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip(scene.Count<DDGISceneSettings>() > 0 ? "The scene already has a GI volume." : "Adds DDGI settings fitted to the current scene bounds.");

	ImGui::EndDisabled();

	ImGui::EndMenu();
}


void MenubarWidget::DrawPlayMenu()
{
	if (!ImGui::BeginMenu("Play"))
		return;

	const EGameState game_state = m_Editor->GetGameState();

	if (ImGui::MenuItem(game_state == GAME_PAUSED ? "Resume" : "Play", "F5", false, game_state != GAME_RUNNING))
	{
		if (game_state == GAME_PAUSED)
			m_Editor->Unpause();
		else
			m_Editor->Start();
	}

	if (ImGui::MenuItem("Pause", nullptr, false, game_state == GAME_RUNNING))
		m_Editor->Pause();

	if (ImGui::MenuItem("Stop", "Escape", false, game_state != GAME_STOPPED))
		m_Editor->Stop();

	ImGui::EndMenu();
}


void MenubarWidget::DrawToolsMenu()
{
	if (!ImGui::BeginMenu("Tools"))
		return;

	if (ImGui::MenuItem((const char*)ICON_FA_CODE "  Shader Editor"))
	{
		const String exe = OS::sGetExecutablePath().string();
		ShellExecute(0, 0, exe.c_str(), "-shader_editor", 0, SW_SHOW);
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Opens the shader graph editor in a new window.");

	if (ImGui::MenuItem((const char*)ICON_FA_LAYER_GROUP "  Asset Compiler"))
	{
		const String exe = OS::sGetExecutablePath().string();
		ShellExecute(0, 0, exe.c_str(), "-asset_compiler", 0, SW_SHOW);
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Starts the asset compiler, it keeps converting assets in the background from the system tray.");

	ImGui::Separator();

	if (ImGui::MenuItem((const char*)ICON_FA_HAMMER "  Build Scripts", "Ctrl+B", false, !m_Editor->IsBuildingScripts()))
		m_Editor->BuildScripts();

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Compiles Code/Game/Scripts, the editor reloads them as soon as the build finishes.");

	if (ImGui::MenuItem((const char*)ICON_FA_SYNC "  Reload Scripts", nullptr, false, !m_Editor->IsBuildingScripts()))
		m_Editor->ReloadScripts();

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Reloads Scripts.dll, script variables are kept.");

	ImGui::EndMenu();
}


void MenubarWidget::DrawHelpMenu()
{
	if (!ImGui::BeginMenu("Help"))
		return;

	if (ImGui::MenuItem((const char*)ICON_FA_KEYBOARD "  Keyboard Shortcuts"))
		m_ShowShortcuts = true;

	if (ImGui::MenuItem((const char*)ICON_FA_QUESTION_CIRCLE "  About"))
		m_ShowAbout = true;

	ImGui::EndMenu();
}


void MenubarWidget::DrawShortcutsWindow()
{
	if (!m_ShowShortcuts)
		return;

	static constexpr std::array cShortcuts =
	{
		std::pair { "Scene", "" },
		std::pair { "Ctrl+N", "New scene" },
		std::pair { "Ctrl+O", "Open scene" },
		std::pair { "Ctrl+S", "Save scene" },
		std::pair { "Ctrl+Shift+S", "Save scene as" },
		std::pair { "Ctrl+I", "Import a model or scene" },
		std::pair { "Editing", "" },
		std::pair { "Ctrl+Z / Ctrl+Y", "Undo / Redo" },
		std::pair { "Ctrl+D", "Duplicate the selected entity" },
		std::pair { "Delete", "Delete the selected entities" },
		std::pair { "Escape", "Deselect, or stop the game" },
		std::pair { "T / R / S", "Move / Rotate / Scale gizmo" },
		std::pair { "Ctrl (hold)", "Snap the gizmo" },
		std::pair { "Viewport", "" },
		std::pair { "Right Mouse + WASD", "Fly the camera" },
		std::pair { "Middle Mouse", "Pan the camera" },
		std::pair { "Ctrl + Middle Mouse", "Orbit the camera" },
		std::pair { "Mouse Wheel", "Zoom the camera" },
		std::pair { "Shift (hold)", "Move the camera faster" },
		std::pair { "Alt", "Toggle mouse capture" },
		std::pair { "F1", "Maximize the viewport" },
		std::pair { "F5", "Play or resume the game" },
		std::pair { "Scripts", "" },
		std::pair { "Ctrl+B", "Build and hot reload scripts" },
	};

	ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 28.0f, 0.0f), ImGuiCond_Appearing);
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::Begin((const char*)ICON_FA_KEYBOARD "  Keyboard Shortcuts", &m_ShowShortcuts, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
	{
		if (ImGui::BeginTable("##Shortcuts", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
		{
			for (const auto& [keys, description] : cShortcuts)
			{
				ImGui::TableNextRow();

				if (description[0] == '\0')
				{
					ImGui::TableSetColumnIndex(0);
					ImGui::Spacing();
					ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", keys);
					continue;
				}

				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(keys);
				ImGui::TableSetColumnIndex(1);
				ImGui::TextDisabled("%s", description);
			}

			ImGui::EndTable();
		}
	}

	ImGui::End();
}


void MenubarWidget::DrawAboutWindow()
{
	if (!m_ShowAbout)
		return;

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::Begin("About Raekor", &m_ShowAbout, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
	{
		const GPUInfo& gpu_info = GetRenderInterface().GetGPUInfo();

		ImGui::Text("Raekor Engine");
		ImGui::TextDisabled("A real-time rendering focused game engine.");
		ImGui::Separator();
		ImGui::Text("GPU: %s", gpu_info.mProduct.c_str());
		ImGui::Text("Vendor: %s", gpu_info.mVendor.c_str());
		ImGui::Text("API: %s", gpu_info.mActiveAPI.c_str());
		ImGui::Text("Dear ImGui: %s", IMGUI_VERSION);
	}

	ImGui::End();
}


void MenubarWidget::SaveScreenshot()
{
	const String save_path = OS::sSaveFileDialog("Uncompressed PNG (*.png) ", "png");

	if (!save_path.empty())
		GetRenderInterface().RequestScreenshot(save_path);
}

} // raekor
