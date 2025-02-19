#include "pch.h"
#include "compiler.h"

#include "OS.h"
#include "GUI.h"
#include "OBJ.h"
#include "FBX.h"
#include "GLTF.h"
#include "Iter.h"
#include "Timer.h"
#include "Assimp.h"
#include "Archive.h"
#include "Threading.h"
#include "Components.h"

#define WIN_TRAY_MESSAGE WM_USER + 1
#define IDI_ICON1 102
#define IDM_MENUITEM1 1
#define IDM_MENUITEM2 2
#define IDM_EXIT 100

#include <winioctl.h>

namespace RK {

CompilerApp::CompilerApp(WindowFlags inFlags) : Application(inFlags | WindowFlag::RESIZE)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui::GetIO().IniFilename = "";

	if (!m_ConfigSettings.mFontFile.empty())
		GUI::SetFont(m_ConfigSettings.mFontFile.string());

	GUI::SetDarkTheme();
	ImGui::GetStyle().ScaleAllSizes(1.33333333f);

	m_Renderer = SDL_CreateRenderer(m_Window, NULL);
	std::cout << "Created SDL_Renderer with name: \"" << SDL_GetRendererName(m_Renderer) << "\"\n";

	ImGui_ImplSDL3_InitForSDLRenderer(m_Window, m_Renderer);
	ImGui_ImplSDLRenderer3_Init(m_Renderer);
	SDL_SetWindowTitle(m_Window, "RK Compiler App");

	SDL_GL_SetSwapInterval(1);

    SDL_PropertiesID props = SDL_GetWindowProperties(m_Window);
    HWND hwnd = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

	// Add the system tray icon
	NOTIFYICONDATA nid = { sizeof(NOTIFYICONDATA) };
	nid.hWnd = hwnd;
	nid.uID = 1;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WIN_TRAY_MESSAGE;
	nid.hIcon = (HICON)GetClassLongPtr(hwnd, -14);
	strcpy(nid.szTip, "RK Compiler App");

	bool ret = Shell_NotifyIcon(NIM_ADD, &nid);

	if (!ret)
	{
		ret = Shell_NotifyIcon(NIM_DELETE, &nid);
		assert(ret);
		ret = Shell_NotifyIcon(NIM_ADD, &nid);
	}

#ifdef NDEBUG
	// hide the console window
	// ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif

	SDL_SetWindowTitle(m_Window, "RK Asset Compiler");

	for (const fs::directory_entry& file : fs::recursive_directory_iterator("assets"))
	{
		if (!file.is_regular_file())
			continue;

		const Path path = file.path();
		const Path extension = path.extension();

		const AssetType asset_type = GetCacheFileExtension(file);
		if (asset_type == ASSET_TYPE_NONE)
			continue;

		// if (m_Files.size() > 65)
		//     return;

		FileEntry& file_entry = m_Files.emplace_back(file);
		file_entry.ReadMetadata();
	}

	g_ThreadPool.SetActiveThreadCount(std::max(2u, g_ThreadPool.GetThreadCount() - 1));

	g_ThreadPool.QueueJob([this]() 
	{
		for (FileEntry& file : m_Files)
			file.UpdateFileHash();
	});

	stbi_set_flip_vertically_on_load(true);

	String assets_folder = fs::absolute("assets").string();
	HANDLE change_notifs = FindFirstChangeNotificationA(assets_folder.c_str(), true, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE);
	assert(change_notifs != INVALID_HANDLE_VALUE);

	m_StartTicks = Timer::sGetCurrentTick();
	m_FinishedTicks = Timer::sGetCurrentTick();
}


#pragma warning(disable:4722)

CompilerApp::~CompilerApp()
{
	NOTIFYICONDATA nid = { sizeof(NOTIFYICONDATA) };
	nid.uID = 1;
	nid.hWnd = GetWindowHandle();
	Shell_NotifyIcon(NIM_DELETE, &nid);

	std::quick_exit(0);
}

#pragma warning(default:4722)


void CompilerApp::OnUpdate(float inDeltaTime)
{
	// SDL_SetWindowTitle(m_Window, std::string(std::to_string(Timer::sToMilliseconds(inDeltaTime)) + " ms.").c_str());

	ImGui_ImplSDL3_NewFrame();
	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui::NewFrame();

	if (ImGui::BeginMainMenuBar())
	{
		ImGui::Text(reinterpret_cast<const char*>( ICON_FA_ADDRESS_BOOK ));

		if (ImGui::BeginMenu("File"))
		{

			if (ImGui::MenuItem("Clear"))
				fs::remove_all(fs::current_path() / "cached");

			if (ImGui::MenuItem("Exit"))
				m_Running = false;

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetCursorPos().y - ImGui::GetFontSize() * 0.5), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

	const ImVec2 frame_padding = ImGui::GetStyle().FramePadding;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(frame_padding.x, frame_padding.y * 0.5));

	ImGuiWindowFlags window_flags = 
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings;

	bool open = true;
	ImGui::Begin("##Compiler", &open, window_flags);

	bool compile_scenes = m_CompileScenes.load();
	if (ImGui::Checkbox("Convert Models", &compile_scenes))
		m_CompileScenes.store(compile_scenes);

	ImGui::SameLine();

	bool compile_textures = m_CompileTextures.load();
	if (ImGui::Checkbox("Convert Textures", &compile_textures))
		m_CompileTextures.store(compile_textures);

	ImGui::SameLine();

	bool compile_scripts = m_CompileScripts.load();
	if (ImGui::Checkbox("Convert Scripts", &compile_scripts))
		m_CompileScripts.store(compile_scripts);

	if (m_FilesInFlight.size() > 0)
	{
		ImGui::SameLine();
		ImGui::Spinner("##filesinflightspinner", ImGui::GetFontSize() / 2.0f, 2, ImGui::GetColorU32(ImGuiCol_CheckMark));
		m_FinishedTicks = Timer::sGetCurrentTick();
	}
	else
	{
		ImGui::SameLine();
		ImGui::Text("Compilation took %.2f seconds.", Timer::sGetTicksToSeconds(m_FinishedTicks - m_StartTicks));
	}

	ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable;
	ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.15, 0.15, 0.15, 1.0));
	ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.20, 0.20, 0.20, 1.0));

	if (ImGui::BeginTable("Assets", 4, table_flags))
	{
		ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
		ImGui::TableSetupColumn("Asset File Path");
		// ImGui::TableSetupColumn("Asset Type");
		ImGui::TableSetupColumn("Cached File Path");
		ImGui::TableSetupColumn("FNV-1a File Hash");
		ImGui::TableSetupColumn("File Modified Time");
		ImGui::TableHeadersRow();

		for (const auto& [index, file] : gEnumerate(m_Files))
		{
			// Sort our data if sort specs have been changed!
			if (ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs())
			{
				if (sorts_specs->SpecsDirty)
				{
					for (int n = 0; n < sorts_specs->SpecsCount; n++)
					{
						const ImGuiTableColumnSortSpecs* sort_spec = &sorts_specs->Specs[n];
						switch (sort_spec->ColumnIndex)
						{
							case 0:
							{
								if (sort_spec->SortDirection == ImGuiSortDirection_Ascending)
									std::sort(m_Files.begin(), m_Files.end(), [](const FileEntry& lhs, const FileEntry& rhs) { return lhs.mAssetPath < rhs.mAssetPath; });
								else
									std::sort(m_Files.begin(), m_Files.end(), [](const FileEntry& lhs, const FileEntry& rhs) { return lhs.mAssetPath > rhs.mAssetPath; });
								break;
							};
							case 1:
							{
								if (sort_spec->SortDirection == ImGuiSortDirection_Ascending)
									std::sort(m_Files.begin(), m_Files.end(), [](const FileEntry& lhs, const FileEntry& rhs) { return lhs.mCachePath < rhs.mCachePath; });
								else
									std::sort(m_Files.begin(), m_Files.end(), [](const FileEntry& lhs, const FileEntry& rhs) { return lhs.mCachePath > rhs.mCachePath; });
								break;
							};
							case 2:
							{
								if (sort_spec->SortDirection == ImGuiSortDirection_Ascending)
									std::sort(m_Files.begin(), m_Files.end(), [](const FileEntry& lhs, const FileEntry& rhs) { return lhs.mFileHash > rhs.mFileHash; });
								else
									std::sort(m_Files.begin(), m_Files.end(), [](const FileEntry& lhs, const FileEntry& rhs) { return lhs.mFileHash < rhs.mFileHash; });
								break;
							};
							case 3:
							{
								if (sort_spec->SortDirection == ImGuiSortDirection_Ascending)
									std::sort(m_Files.begin(), m_Files.end(), [](const FileEntry& lhs, const FileEntry& rhs) { return lhs.mWriteTime < rhs.mWriteTime; });
								else
									std::sort(m_Files.begin(), m_Files.end(), [](const FileEntry& lhs, const FileEntry& rhs) { return lhs.mWriteTime > rhs.mWriteTime; });
								break;
							};
							default: assert(false);
						}
					}

					//return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? +1 : -1;
					//return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? -1 : +1;

					sorts_specs->SpecsDirty = false;
				}
			}

			ImGui::TableNextRow();

			ImGui::TableNextColumn();

			ImGui::PushID(index);
			ImGui::Selectable("##row", m_SelectedIndex == index, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick);
			ImGui::PopID();

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, frame_padding * 2);

			if (ImGui::BeginPopupContextItem())
			{
				m_SelectedIndex = index;

				if (ImGui::MenuItem("Open.."))
				{
					if (fs::exists(file.mCachePath))
						ShellExecute(NULL, "open", file.mCachePath.c_str(), NULL, NULL, SW_RESTORE);
					else
						ShellExecute(NULL, "open", file.mAssetPath.c_str(), NULL, NULL, SW_RESTORE);
				}

				if (ImGui::MenuItem("Delete", "DELETE"))
				{
					const FileEntry& file = m_Files[m_SelectedIndex];

					std::error_code error_code;
					if (fs::exists(file.mCachePath, error_code))
						fs::remove(file.mCachePath);
				}

				if (ImGui::MenuItem("Open Containing Folder.."))
				{
					String filepath = fs::exists(file.mCachePath) ? file.mCachePath : file.mAssetPath;
					String folder = Path(filepath).parent_path().string();
					ShellExecute(NULL, "open", folder.c_str(), NULL, NULL, SW_RESTORE);
				}

				ImGui::EndPopup();
			}
			else if (ImGui::IsItemHovered())
			{
				if (ImGui::IsMouseDoubleClicked(0))
				{
					if (fs::exists(file.mCachePath))
						ShellExecute(NULL, "open", file.mCachePath.c_str(), NULL, NULL, SW_RESTORE);
					else
						ShellExecute(NULL, "open", file.mAssetPath.c_str(), NULL, NULL, SW_RESTORE);
				}
				else if (ImGui::IsMouseClicked(0))
					m_SelectedIndex = index;
			}

			ImGui::PopStyleVar();

			ImGui::SameLine();

			if (file.mIsCached)
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
			}

			ImGui::Text(reinterpret_cast<const char*>( ICON_FA_CLOUD ));

			ImGui::PopStyleColor();

			ImGui::SameLine();

			ImGui::Text(file.mAssetPath.c_str());

			//ImGui::TableNextColumn();

			//switch (file.mAssetType) {
			//    case ASSET_TYPE_IMAGE: ImGui::Text("Image"); break;
			//    case ASSET_TYPE_SCENE: ImGui::Text("Scene"); break;
			//    default:
			//        assert(false);
			//}

			ImGui::TableNextColumn();

			ImGui::Text(file.mCachePath.c_str());

			ImGui::TableNextColumn();

			ImGui::Text("Hash: %u", file.mFileHash);

			ImGui::TableNextColumn();

			ImGui::Text("Write time: %s", asctime(gmtime(&file.mWriteTime)));

		}

		ImGui::EndTable();
	}

	ImGui::NewLine();

	ImGui::PopStyleColor();
	ImGui::PopStyleColor();

	ImGui::End(); // ##Window
	ImGui::PopStyleVar(1); // ImGuiStyleVar_WindowRounding 0.0f
	ImGui::PopStyleVar(1); // ImGuiStyleVar_WindowRounding 0.0f

	ImGui::EndFrame();
	ImGui::Render();

	SDL_SetRenderDrawColor(m_Renderer, 0, 0, 0, 0);
	SDL_RenderClear(m_Renderer);

	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_Renderer);

	SDL_RenderPresent(m_Renderer);

	for (auto [index, file] : gEnumerate(m_Files))
	{
		if (file.mIsCached || m_FilesInFlight.contains(index))
			continue;

		if (file.mAssetType == ASSET_TYPE_IMAGE && m_CompileTextures)
		{
			m_FilesInFlight.insert(index);

			g_ThreadPool.QueueJob([this, index, &file]()
			{
				if (Path(file.mAssetPath).extension() != ".dds")
				{
					TextureAsset::Convert(file.mAssetPath);
				}
				else
				{
					fs::create_directories(Path(file.mCachePath).parent_path());
					fs::copy_file(file.mAssetPath, file.mCachePath);
				}

				// conversion may have failed, but we don't want to keep trying to convert, so mark as cached
				//file.UpdateWriteTime();
				file.mIsCached = true;

				std::scoped_lock lock(m_FilesInFlightMutex);
				m_FilesInFlight.erase(index);
				LogMessage(std::format("[Assets] Converted {}", file.mAssetPath));
			});
		}
		else if (file.mAssetType == ASSET_TYPE_EMBEDDED)
		{

		}
		else if (file.mAssetType == ASSET_TYPE_CPP_SCRIPT && m_CompileScripts)
		{
			m_FilesInFlight.insert(index);

			g_ThreadPool.QueueJob([this, index, &file]()
			{
				fs::create_directories(Path(file.mCachePath).parent_path());

				const String clang_exe = "dependencies\\clang\\clang.exe";
				const String includes = "-I source\\RK\\ -I dependencies\\BinaryRelations -I dependencies\\cgltf -I dependencies\\glm\\glm -I dependencies\\JoltPhysics -I build\\vcpkg_installed\\x64-windows-static\\include";
				const String command = std::format("{} -gcodeview {} {} -shared -std=c++20 -o {}", clang_exe, includes, file.mAssetPath, file.mCachePath);

				OS::sCreateProcess(command.c_str());

				file.ReadMetadata();

				std::scoped_lock lock(m_FilesInFlightMutex);
				m_FilesInFlight.erase(index);
				LogMessage(std::format("[Assets] Converted {}", file.mAssetPath));
			});
		}
		else if (file.mAssetType == ASSET_TYPE_SCENE && m_CompileScenes)
		{
			m_FilesInFlight.insert(index);

			g_ThreadPool.QueueJob([this, index, &file]()
			{
				Assets assets;
				Scene scene(nullptr); // nullptr, dont need a renderer

				const Path extension = Path(file.mAssetPath).extension();

				if (extension == ".fbx")
				{
					FBXImporter importer(scene, nullptr);
					importer.LoadFromFile(file.mAssetPath, nullptr);
				}
				else if (extension == ".gltf")
				{
					GltfImporter importer(scene, nullptr);
					importer.LoadFromFile(file.mAssetPath, nullptr);
				}
				else if (extension == ".obj")
				{
					OBJImporter importer(scene, nullptr);
					importer.LoadFromFile(file.mAssetPath, nullptr);
				}
#ifndef DEPRECATE_ASSIMP
				else
				{
					auto importer = AssimpImporter(scene, nullptr);
					importer.LoadFromFile(file.mAssetPath, nullptr);
				}
#endif

				fs::create_directories(Path(file.mCachePath).parent_path());

				if (scene.Count<DirectionalLight>() == 0)
					scene.Add<DirectionalLight>(scene.CreateSpatialEntity("Directional Light"));

				scene.SaveToFile(file.mCachePath, assets);

				file.ReadMetadata();

				std::scoped_lock lock(m_FilesInFlightMutex);
				m_FilesInFlight.erase(index);
				LogMessage(std::format("[Assets] Converted {}", file.mAssetPath));
			});
		}
	}
}



void CompilerApp::OnEvent(const SDL_Event& inEvent)
{
	ImGui_ImplSDL3_ProcessEvent(&inEvent);

	if (inEvent.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
		m_WasClosed = true;

	if (inEvent.type == SDL_EVENT_WINDOW_MINIMIZED)
		SDL_HideWindow(m_Window);

	if (inEvent.type == SDL_EVENT_KEY_DOWN && !inEvent.key.repeat)
	{
		switch (inEvent.key.key)
		{
			case SDLK_DELETE:
			{
				if (m_SelectedIndex > 0 && m_SelectedIndex < m_Files.size())
				{
					const FileEntry& file = m_Files[m_SelectedIndex];

					std::error_code error_code;
					if (fs::exists(file.mCachePath, error_code))
						fs::remove(file.mCachePath);
				}

			} break;
		}
	}

}


void CompilerApp::LogMessage(const std::string& inMessage)
{
	Application::LogMessage(inMessage);

	COPYDATASTRUCT cds = {};
	cds.dwData = IPC::LOG_MESSAGE_SENT;
	cds.lpData = (PVOID)inMessage.c_str();
	cds.cbData = inMessage.size() + 1;

    HWND hwnd = GetWindowHandle();
	HWND parent = GetAncestor(hwnd, GA_PARENT);
	SendMessage(parent, WM_COPYDATA, (WPARAM)(HWND)hwnd, (LPARAM)(LPVOID)&cds);
}


void CompilerApp::OpenFromTray()
{
	SDL_ShowWindow(m_Window);
	SDL_RestoreWindow(m_Window);
	SDL_RaiseWindow(m_Window);
	ShowWindow(GetWindowHandle(), SW_RESTORE);
}


HWND CompilerApp::GetWindowHandle()
{
    SDL_PropertiesID props = SDL_GetWindowProperties(m_Window);
    return (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

}

}