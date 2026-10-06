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
#include <commctrl.h>

namespace RK {

static constexpr std::array cAssetTypeNames = { "Model", "Texture", "Embedded", "Script" };

static constexpr ImVec4 cConvertedColor = ImVec4(0.36f, 0.80f, 0.45f, 1.0f);
static constexpr ImVec4 cPendingColor   = ImVec4(0.95f, 0.72f, 0.30f, 1.0f);
static constexpr ImVec4 cBusyColor      = ImVec4(0.34f, 0.60f, 0.98f, 1.0f);


static LRESULT CALLBACK sTrayWindowProc(HWND inWindow, UINT inMessage, WPARAM inWParam, LPARAM inLParam, UINT_PTR inSubclassID, DWORD_PTR inRefData)
{
	if (inMessage == WIN_TRAY_MESSAGE)
	{
		CompilerApp* app = (CompilerApp*)inRefData;

		switch (LOWORD(inLParam))
		{
			case WM_LBUTTONUP:
			case WM_LBUTTONDBLCLK:
				app->OpenFromTray();
				return 0;

			case WM_RBUTTONUP:
			case WM_CONTEXTMENU:
				app->ShowTrayMenu();
				return 0;
		}
	}

	return DefSubclassProc(inWindow, inMessage, inWParam, inLParam);
}


CompilerApp::CompilerApp(WindowFlags inFlags) : Application(inFlags | WindowFlag::RESIZE)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().IniFilename = "";

	const float ui_scale = GUI::GetDisplayScale(m_Window);

	GUI::SetDarkTheme(ui_scale);

	if (!m_ConfigSettings.mFontFile.empty())
		GUI::SetFont(m_ConfigSettings.mFontFile.string(), ui_scale);

	const String ipc_window_value = OS::sGetCommandLineValue("-ipc_window");

	if (!ipc_window_value.empty())
	{
		const HWND ipc_window = (HWND)std::stoull(ipc_window_value);

		m_IPCLogSink = g_Logger.AddSink([ipc_window](const LogMessage& inMessage)
		{
			const String data = gSerializeLogMessage(inMessage);

			COPYDATASTRUCT cds = {};
			cds.dwData = IPC::LOG_MESSAGE_SENT;
			cds.lpData = (PVOID)data.data();
			cds.cbData = DWORD(data.size());

			DWORD_PTR result = 0;
			SendMessageTimeoutA(ipc_window, WM_COPYDATA, 0, (LPARAM)&cds, SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, &result);
		});
	}

	m_Renderer = SDL_CreateRenderer(m_Window, NULL);
	gLogInfo("Asset Compiler", "Created SDL_Renderer with name \"{}\"", SDL_GetRendererName(m_Renderer));

	ImGui_ImplSDL3_InitForSDLRenderer(m_Window, m_Renderer);
	ImGui_ImplSDLRenderer3_Init(m_Renderer);

	SDL_SetRenderVSync(m_Renderer, 1);
	SDL_SetWindowTitle(m_Window, "RK Asset Compiler");

	const HWND hwnd = GetWindowHandle();

	// Add the system tray icon
	NOTIFYICONDATA nid = { sizeof(NOTIFYICONDATA) };
	nid.hWnd = hwnd;
	nid.uID = 1;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WIN_TRAY_MESSAGE;
	nid.hIcon = (HICON)GetClassLongPtr(hwnd, -14);
	strcpy(nid.szTip, "RK Asset Compiler");

	bool ret = Shell_NotifyIcon(NIM_ADD, &nid);

	if (!ret)
	{
		ret = Shell_NotifyIcon(NIM_DELETE, &nid);
		assert(ret);
		ret = Shell_NotifyIcon(NIM_ADD, &nid);
	}

	SetWindowSubclass(hwnd, sTrayWindowProc, 1, (DWORD_PTR)this);

	for (const fs::directory_entry& file : fs::recursive_directory_iterator("assets"))
	{
		if (!file.is_regular_file())
			continue;

		const AssetType asset_type = GetCacheFileExtension(file);
		if (asset_type == ASSET_TYPE_NONE)
			continue;

		FileEntry& file_entry = m_Files.emplace_back(file);
		file_entry.ReadMetadata();
	}

	m_SortedFiles.resize(m_Files.size());
	std::iota(m_SortedFiles.begin(), m_SortedFiles.end(), 0u);

	g_JobSystem.SetActiveThreadCount(std::max(2u, g_JobSystem.GetThreadCount() - 1));

	g_JobSystem.Schedule([this]()
	{
		for (FileEntry& file : m_Files)
			file.UpdateFileHash();
	}, JOB_PRIORITY_LOW);

	stbi_set_flip_vertically_on_load(true);

	m_StartTicks = Timer::sGetCurrentTick();
	m_FinishedTicks = Timer::sGetCurrentTick();
}


#pragma warning(disable:4722)

CompilerApp::~CompilerApp()
{
	if (m_IPCLogSink)
		g_Logger.RemoveSink(m_IPCLogSink);

	RemoveWindowSubclass(GetWindowHandle(), sTrayWindowProc, 1);

	NOTIFYICONDATA nid = { sizeof(NOTIFYICONDATA) };
	nid.uID = 1;
	nid.hWnd = GetWindowHandle();
	Shell_NotifyIcon(NIM_DELETE, &nid);

	std::quick_exit(0);
}

#pragma warning(default:4722)


void CompilerApp::OnUpdate(float inDeltaTime)
{
	ScheduleCompilation();

	if (SDL_GetWindowFlags(m_Window) & ( SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED ))
	{
		SDL_Delay(50);
		return;
	}

	ImGui_ImplSDL3_NewFrame();
	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui::NewFrame();

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);

	const ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImGui::GetStyle().WindowPadding * 1.5f);

	ImGui::Begin("##Compiler", nullptr, window_flags);

	DrawHeader();
	DrawToolbar();
	DrawFileTable();
	DrawClearCachePopup();

	ImGui::End();
	ImGui::PopStyleVar(3);

	ImGui::Render();

	const ImVec4 clear_color = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
	SDL_SetRenderDrawColorFloat(m_Renderer, clear_color.x, clear_color.y, clear_color.z, 1.0f);
	SDL_RenderClear(m_Renderer);

	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_Renderer);

	SDL_RenderPresent(m_Renderer);
}


void CompilerApp::DrawHeader()
{
	uint32_t converted_count = 0;
	uint32_t pending_count = 0;
	uint32_t busy_count = 0;

	{
		std::scoped_lock lock(m_FilesInFlightMutex);

		busy_count = uint32_t(m_FilesInFlight.size());

		for (const FileEntry& file : m_Files)
		{
			if (file.mIsCached)
				converted_count++;
			else if (IsConversionEnabled(file.mAssetType))
				pending_count++;
		}
	}

	if (busy_count > 0)
		m_FinishedTicks = Timer::sGetCurrentTick();

	const char* title = (const char*)ICON_FA_TOOLS "  Asset Compiler";
	const float title_size = ImGui::GetFontSize() * 1.5f;
	const ImVec2 title_extent = ImGui::GetFont()->CalcTextSizeA(title_size, FLT_MAX, 0.0f, title);

	ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), title_size, ImGui::GetCursorScreenPos(), ImGui::GetColorU32(ImGuiCol_Text), title);
	ImGui::Dummy(title_extent);

	ImGui::TextDisabled("Converts everything in %s into engine ready files in the cache, and keeps doing so in the background from the system tray.", fs::absolute("assets").string().c_str());

	ImGui::Spacing();

	const float stat_width = ImGui::GetFontSize() * 9.0f;

	auto DrawStat = [&](const char* inIcon, const ImVec4& inColor, uint32_t inValue, const char* inLabel)
	{
		ImGui::BeginGroup();
		ImGui::TextColored(inColor, "%s", inIcon);
		ImGui::SameLine();
		ImGui::Text("%u", inValue);
		ImGui::SameLine();
		ImGui::TextDisabled("%s", inLabel);
		ImGui::EndGroup();
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::Dummy(ImVec2(glm::max(stat_width - ImGui::GetItemRectSize().x, 0.0f), 0.0f));
		ImGui::SameLine();
	};

	DrawStat((const char*)ICON_FA_LAYER_GROUP, ImGui::GetStyleColorVec4(ImGuiCol_Text), uint32_t(m_Files.size()), "assets");
	DrawStat((const char*)ICON_FA_CHECK_CIRCLE, cConvertedColor, converted_count, "converted");
	DrawStat((const char*)ICON_FA_CLOCK, cPendingColor, pending_count, "pending");
	DrawStat((const char*)ICON_FA_SYNC, cBusyColor, busy_count, "in progress");
	ImGui::NewLine();

	const uint32_t convertible_count = converted_count + pending_count;
	const float progress = convertible_count > 0 ? float(converted_count) / float(convertible_count) : 1.0f;

	const String progress_text = busy_count > 0 ?
		std::format("Converting.. {:.0f}%", progress * 100.0f) :
		std::format("Up to date, last pass took {:.2f} seconds", Timer::sGetTicksToSeconds(m_FinishedTicks - m_StartTicks));

	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, busy_count > 0 ? cBusyColor : cConvertedColor);
	ImGui::ProgressBar(progress, ImVec2(-FLT_MIN, 0.0f), progress_text.c_str());
	ImGui::PopStyleColor();

	ImGui::Spacing();
}


void CompilerApp::DrawToolbar()
{
	auto DrawToggle = [](const char* inLabel, std::atomic<bool>& ioValue, const char* inTooltip)
	{
		bool value = ioValue.load();

		if (ImGui::Checkbox(inLabel, &value))
			ioValue.store(value);

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", inTooltip);

		ImGui::SameLine();
	};

	DrawToggle("Models", m_CompileScenes, "Convert .gltf, .fbx and .obj files to .scene files");
	DrawToggle("Textures", m_CompileTextures, "Convert images to block compressed .dds files");
	DrawToggle("Scripts", m_CompileScripts, "Compile C++ scripts to hot loadable .dll files");

	ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 3.0f);

	const float clear_button_width = ImGui::CalcTextSize("Clear Cache").x + ImGui::GetFontSize() * 2.0f + ImGui::GetStyle().FramePadding.x * 2.0f;
	const float combo_width = ImGui::GetFontSize() * 8.0f;
	const float filter_width = glm::max(ImGui::GetContentRegionAvail().x - combo_width - clear_button_width - ImGui::GetStyle().ItemSpacing.x * 2.0f, ImGui::GetFontSize() * 8.0f);

	ImGui::SetNextItemWidth(filter_width);
	ImGui::InputTextWithHint("##Filter", (const char*)ICON_FA_SEARCH "  Search assets..", &m_Filter);

	ImGui::SameLine();
	ImGui::SetNextItemWidth(combo_width);

	if (ImGui::BeginCombo("##TypeFilter", m_TypeFilter < 0 ? "All Types" : cAssetTypeNames[m_TypeFilter]))
	{
		if (ImGui::Selectable("All Types", m_TypeFilter < 0))
			m_TypeFilter = -1;

		for (int type = 0; type < int(cAssetTypeNames.size()); type++)
		{
			if (ImGui::Selectable(cAssetTypeNames[type], m_TypeFilter == type))
				m_TypeFilter = type;
		}

		ImGui::EndCombo();
	}

	ImGui::SameLine();

	if (ImGui::Button((const char*)ICON_FA_TRASH "  Clear Cache"))
		m_OpenClearCachePopup = true;

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Delete every converted file so all assets are converted again.");
}


void CompilerApp::DrawFileTable()
{
	const ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable |
										ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchProp;

	if (!ImGui::BeginTable("Assets", 4, table_flags))
		return;

	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_DefaultSort, 0.45f);
	ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_None, 0.1f);
	ImGui::TableSetupColumn("Converted File", ImGuiTableColumnFlags_None, 0.3f);
	ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_None, 0.15f);
	ImGui::TableHeadersRow();

	if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs())
	{
		if (sort_specs->SpecsDirty && sort_specs->SpecsCount > 0)
		{
			const ImGuiTableColumnSortSpecs& spec = sort_specs->Specs[0];
			SortFiles(spec.ColumnIndex, spec.SortDirection == ImGuiSortDirection_Ascending);
			sort_specs->SpecsDirty = false;
		}
	}

	const ImGuiTextFilter filter = ImGuiTextFilter(m_Filter.c_str());

	std::scoped_lock lock(m_FilesInFlightMutex);

	for (uint32_t index : m_SortedFiles)
	{
		const FileEntry& file = m_Files[index];

		if (m_TypeFilter >= 0 && int(file.mAssetType) != m_TypeFilter)
			continue;

		if (!filter.PassFilter(file.mAssetPath.c_str()))
			continue;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		ImGui::PushID(int(index));

		if (ImGui::Selectable("##row", m_SelectedIndex == int(index), ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowOverlap))
		{
			m_SelectedIndex = int(index);

			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				OpenFile(file);
		}

		if (ImGui::BeginPopupContextItem("##FileContext"))
		{
			m_SelectedIndex = int(index);

			if (ImGui::MenuItem((const char*)ICON_FA_EXTERNAL_LINK_ALT "  Open"))
				OpenFile(file);

			if (ImGui::MenuItem((const char*)ICON_FA_FOLDER_OPEN "  Show in Explorer"))
				ShowInExplorer(file);

			ImGui::Separator();

			if (ImGui::MenuItem((const char*)ICON_FA_REDO_ALT "  Convert Again", "Delete", false, file.mIsCached && !m_FilesInFlight.contains(index)))
				Recompile(index);

			ImGui::EndPopup();
		}

		ImGui::PopID();

		const char* status_icon = (const char*)ICON_FA_CLOCK;
		ImVec4 status_color = cPendingColor;
		const char* status_text = "Waiting to be converted";

		if (m_FilesInFlight.contains(index))
		{
			status_icon = (const char*)ICON_FA_SYNC;
			status_color = cBusyColor;
			status_text = "Converting..";
		}
		else if (file.mIsCached)
		{
			status_icon = (const char*)ICON_FA_CHECK_CIRCLE;
			status_color = cConvertedColor;
			status_text = "Converted";
		}
		else if (file.mAssetType == ASSET_TYPE_EMBEDDED)
		{
			status_icon = (const char*)ICON_FA_MINUS_CIRCLE;
			status_color = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
			status_text = "Used as is, no conversion needed";
		}

		ImGui::SameLine(0.0f, 0.0f);
		ImGui::TextColored(status_color, "%s", status_icon);

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", status_text);

		ImGui::SameLine();
		ImGui::TextUnformatted(file.mAssetPath.c_str());

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
			ImGui::SetTooltip("%s\nFNV-1a hash: %llu", file.mAssetPath.c_str(), file.mFileHash);

		ImGui::TableNextColumn();
		ImGui::TextDisabled("%s", cAssetTypeNames[file.mAssetType]);

		ImGui::TableNextColumn();

		if (file.mIsCached)
			ImGui::TextUnformatted(file.mCachePath.c_str());
		else
			ImGui::TextDisabled("-");

		ImGui::TableNextColumn();

		std::tm local_time = {};
		localtime_s(&local_time, &file.mWriteTime);

		char time_buffer[64] = {};
		std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M", &local_time);

		ImGui::TextDisabled("%s", time_buffer);
	}

	ImGui::EndTable();
}


void CompilerApp::DrawClearCachePopup()
{
	if (m_OpenClearCachePopup)
	{
		ImGui::OpenPopup("Clear Cache");
		m_OpenClearCachePopup = false;
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Clear Cache", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::TextUnformatted((const char*)ICON_FA_EXCLAMATION_TRIANGLE "  Delete every converted file?");
		ImGui::TextDisabled("All assets will be converted again, this can take a while for large projects.");

		ImGui::Spacing();

		bool can_clear = false;

		{
			std::scoped_lock lock(m_FilesInFlightMutex);
			can_clear = m_FilesInFlight.empty();
		}

		ImGui::BeginDisabled(!can_clear);

		if (ImGui::Button("Delete", ImVec2(ImGui::GetFontSize() * 7.0f, 0.0f)))
		{
			std::error_code error_code;
			fs::remove_all(fs::current_path() / "cached", error_code);

			if (error_code)
				gLogError("Asset Compiler", "Failed to clear the cache: {}", error_code.message());

			std::scoped_lock lock(m_FilesInFlightMutex);

			for (FileEntry& file : m_Files)
				file.ReadMetadata();

			m_StartTicks = Timer::sGetCurrentTick();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndDisabled();

		if (!can_clear && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Wait for the current conversions to finish first.");

		ImGui::SameLine();

		if (ImGui::Button("Cancel", ImVec2(ImGui::GetFontSize() * 7.0f, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}
}


void CompilerApp::SortFiles(int inColumn, bool inAscending)
{
	auto Compare = [this, inColumn](uint32_t inLeft, uint32_t inRight)
	{
		const FileEntry& lhs = m_Files[inLeft];
		const FileEntry& rhs = m_Files[inRight];

		switch (inColumn)
		{
			case 1:  return lhs.mAssetType != rhs.mAssetType ? lhs.mAssetType < rhs.mAssetType : lhs.mAssetPath < rhs.mAssetPath;
			case 2:  return lhs.mCachePath < rhs.mCachePath;
			case 3:  return lhs.mWriteTime < rhs.mWriteTime;
			default: return lhs.mAssetPath < rhs.mAssetPath;
		}
	};

	std::scoped_lock lock(m_FilesInFlightMutex);

	if (inAscending)
		std::stable_sort(m_SortedFiles.begin(), m_SortedFiles.end(), Compare);
	else
		std::stable_sort(m_SortedFiles.begin(), m_SortedFiles.end(), [&](uint32_t inLeft, uint32_t inRight) { return Compare(inRight, inLeft); });
}


void CompilerApp::ScheduleCompilation()
{
	std::scoped_lock lock(m_FilesInFlightMutex);

	for (auto [index, file] : gEnumerate(m_Files))
	{
		if (file.mIsCached || m_FilesInFlight.contains(index) || !IsConversionEnabled(file.mAssetType))
			continue;

		if (m_FilesInFlight.empty())
			m_StartTicks = Timer::sGetCurrentTick();

		m_FilesInFlight.insert(index);

		if (file.mAssetType == ASSET_TYPE_IMAGE)
		{
			g_JobSystem.Schedule([this, index, &file]()
			{
				if (Path(file.mAssetPath).extension() != ".dds")
				{
					TextureAsset::Convert(file.mAssetPath);
				}
				else
				{
					std::error_code error_code;
					fs::create_directories(Path(file.mCachePath).parent_path());
					fs::copy_file(file.mAssetPath, file.mCachePath, fs::copy_options::overwrite_existing, error_code);
				}

				std::scoped_lock lock(m_FilesInFlightMutex);

				// conversion may have failed, but we don't want to keep trying to convert, so mark as cached
				file.mIsCached = true;
				m_FilesInFlight.erase(index);

				gLogInfo("Assets", "Converted {}", file.mAssetPath);
			});
		}
		else if (file.mAssetType == ASSET_TYPE_CPP_SCRIPT)
		{
			g_JobSystem.Schedule([this, index, &file]()
			{
				fs::create_directories(Path(file.mCachePath).parent_path());

				const String clang_exe = "dependencies\\clang\\clang.exe";
				const String includes = "-I source\\RK\\ -I dependencies\\BinaryRelations -I dependencies\\cgltf -I dependencies\\glm\\glm -I dependencies\\JoltPhysics -I build\\vcpkg_installed\\x64-windows-static\\include";
				const String command = std::format("{} -gcodeview {} {} -shared -std=c++20 -o {}", clang_exe, includes, file.mAssetPath, file.mCachePath);

				OS::sCreateProcess(command.c_str());

				std::scoped_lock lock(m_FilesInFlightMutex);

				file.ReadMetadata();
				m_FilesInFlight.erase(index);

				gLogInfo("Assets", "Converted {}", file.mAssetPath);
			});
		}
		else if (file.mAssetType == ASSET_TYPE_SCENE)
		{
			g_JobSystem.Schedule([this, index, &file]()
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

				std::scoped_lock lock(m_FilesInFlightMutex);

				file.ReadMetadata();
				m_FilesInFlight.erase(index);

				gLogInfo("Assets", "Converted {}", file.mAssetPath);
			});
		}
	}
}


bool CompilerApp::IsConversionEnabled(AssetType inType) const
{
	switch (inType)
	{
		case ASSET_TYPE_SCENE:      return m_CompileScenes;
		case ASSET_TYPE_IMAGE:      return m_CompileTextures;
		case ASSET_TYPE_CPP_SCRIPT: return m_CompileScripts;
		default:                    return false;
	}
}


void CompilerApp::OpenFile(const FileEntry& inFile)
{
	const String& path = fs::exists(inFile.mCachePath) ? inFile.mCachePath : inFile.mAssetPath;
	ShellExecute(NULL, "open", path.c_str(), NULL, NULL, SW_RESTORE);
}


void CompilerApp::ShowInExplorer(const FileEntry& inFile)
{
	const String path = fs::absolute(fs::exists(inFile.mCachePath) ? inFile.mCachePath : inFile.mAssetPath).string();
	const String arguments = std::format("/select,\"{}\"", path);
	ShellExecute(NULL, "open", "explorer.exe", arguments.c_str(), NULL, SW_SHOWNORMAL);
}


void CompilerApp::Recompile(uint32_t inIndex)
{
	FileEntry& file = m_Files[inIndex];

	std::error_code error_code;

	if (fs::exists(file.mCachePath, error_code))
		fs::remove(file.mCachePath, error_code);

	file.ReadMetadata();
}



void CompilerApp::OnEvent(const SDL_Event& inEvent)
{
	ImGui_ImplSDL3_ProcessEvent(&inEvent);

	if (inEvent.type == SDL_EVENT_WINDOW_MINIMIZED)
		SDL_HideWindow(m_Window);

	if (inEvent.type == SDL_EVENT_KEY_DOWN && !inEvent.key.repeat && !ImGui::GetIO().WantTextInput)
	{
		switch (inEvent.key.key)
		{
			case SDLK_DELETE:
			{
				std::scoped_lock lock(m_FilesInFlightMutex);

				if (m_SelectedIndex >= 0 && m_SelectedIndex < int(m_Files.size()) && !m_FilesInFlight.contains(m_SelectedIndex))
					Recompile(m_SelectedIndex);
			} break;
		}
	}
}


bool CompilerApp::OnCloseRequested()
{
	SDL_HideWindow(m_Window);
	gLogInfo("Asset Compiler", "Still running in the system tray, right click the tray icon to exit.");
	return false;
}


void CompilerApp::OpenFromTray()
{
	SDL_ShowWindow(m_Window);
	SDL_RestoreWindow(m_Window);
	SDL_RaiseWindow(m_Window);
	ShowWindow(GetWindowHandle(), SW_RESTORE);
}


void CompilerApp::ShowTrayMenu()
{
	const HWND hwnd = GetWindowHandle();

	HMENU menu = CreatePopupMenu();
	AppendMenuA(menu, MF_STRING, IDM_MENUITEM1, "Open Asset Compiler");
	AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
	AppendMenuA(menu, MF_STRING, IDM_EXIT, "Exit");

	POINT cursor = {};
	GetCursorPos(&cursor);

	SetForegroundWindow(hwnd);
	const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, cursor.x, cursor.y, 0, hwnd, nullptr);
	PostMessage(hwnd, WM_NULL, 0, 0);

	DestroyMenu(menu);

	if (command == IDM_MENUITEM1)
		OpenFromTray();
	else if (command == IDM_EXIT)
		Terminate();
}


HWND CompilerApp::GetWindowHandle()
{
    SDL_PropertiesID props = SDL_GetWindowProperties(m_Window);
    return (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
}

}
