#include "PCH.h"
#include "Launcher.h"
#include "OS.h"
#include "CVars.h"
#include "GUIStyle.h"

namespace RK {

static constexpr const char* cLauncherGameCVar = "launcher_game";
static constexpr const char* cGameScenesDirectory = "Assets/Scenes";


static String sGetGameDisplayName(const Path& inSceneFile)
{
	const String stem = inSceneFile.stem().string();

	String name;

	for (size_t index = 0; index < stem.size(); index++)
	{
		const char current = stem[index];

		if (index > 0)
		{
			const char previous = stem[index - 1];

			const bool lower_to_upper = std::islower(previous) && std::isupper(current);
			const bool letter_to_digit = std::isalpha(previous) && std::isdigit(current);

			if (lower_to_upper || letter_to_digit)
				name += ' ';
		}

		name += current == '_' ? ' ' : current;
	}

	return name;
}


static bool sIsToggleCVar(StringView inName)
{
	size_t start = 0;

	while (start <= inName.size())
	{
		const size_t end = glm::min(inName.find('_', start), inName.size());
		const StringView word = inName.substr(start, end - start);

		if (word == "enable" || word == "disable")
			return true;

		start = end + 1;
	}

	return false;
}


Launcher::Launcher() : Application(WindowFlag::HIDDEN | WindowFlag::RESIZE)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().IniFilename = "";

	// hide the console window
	if (!IsDebuggerPresent())
		ShowWindow(GetConsoleWindow(), SW_HIDE);

	const float ui_scale = GUI::GetDisplayScale(m_Window);

	GUI::SetDarkTheme(ui_scale);

	if (!m_ConfigSettings.mFontFile.empty())
		GUI::SetFont(m_ConfigSettings.mFontFile.string(), ui_scale);

	m_Renderer = SDL_CreateRenderer(m_Window, NULL);
	gLogInfo("Launcher", "Created SDL_Renderer with name \"{}\"", SDL_GetRendererName(m_Renderer));

	ImGui_ImplSDL3_InitForSDLRenderer(m_Window, m_Renderer);
	ImGui_ImplSDLRenderer3_Init(m_Renderer);

	SDL_SetRenderVSync(m_Renderer, 1);
	SDL_SetWindowTitle(m_Window, "RK Launcher");
	SDL_SetWindowMinimumSize(m_Window, int(480 * ui_scale), int(420 * ui_scale));
	SDL_SetWindowSize(m_Window, int(720 * ui_scale), int(640 * ui_scale));

	const String last_game = g_CVariables->Create(cLauncherGameCVar, String());

	for (const auto& [name, cvar] : g_CVariables->GetCVars())
	{
		if (cvar.mType != CVAR_TYPE_FUNCTION && cvar.mType != CVAR_TYPE_NONE && name != cLauncherGameCVar)
			m_SortedCvarNames.push_back(name);
	}

	std::sort(m_SortedCvarNames.begin(), m_SortedCvarNames.end());

	std::error_code error_code;

	for (const fs::directory_entry& entry : fs::directory_iterator(cGameScenesDirectory, error_code))
	{
		if (entry.is_regular_file() && entry.path().extension() == ".scene")
			m_Games.push_back(GameEntry { .mName = sGetGameDisplayName(entry.path()), .mSceneFile = entry.path().generic_string() });
	}

	std::sort(m_Games.begin(), m_Games.end(), [](const GameEntry& inLeft, const GameEntry& inRight) { return inLeft.mName < inRight.mName; });

	if (!m_Games.empty())
		m_SceneFile = m_Games.front().mSceneFile;

	for (const GameEntry& game : m_Games)
	{
		if (game.mSceneFile == Path(last_game))
			m_SceneFile = game.mSceneFile;
	}

	SetDisplay(m_ConfigSettings.mDisplayID);
	SDL_ShowWindow(m_Window);
}



Launcher::~Launcher()
{
	ImGui_ImplSDL3_Shutdown();
	ImGui_ImplSDLRenderer3_Shutdown();
	SDL_DestroyRenderer(m_Renderer);
	ImGui::DestroyContext();
}



void Launcher::OnUpdate(float inDeltaTime)
{
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

	ImGui::Begin("##Launcher", nullptr, window_flags);

	DrawHeader();
	DrawGames();
	DrawStartupSettings();
	DrawConsoleVariables();
	DrawFooter();

	if (!ImGui::GetIO().WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
			Launch();

		if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
			m_Running = false;
	}

	ImGui::End();
	ImGui::PopStyleVar(3);

	ImGui::Render();

	const ImVec4 clear_color = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
	SDL_SetRenderDrawColorFloat(m_Renderer, clear_color.x, clear_color.y, clear_color.z, 1.0f);
	SDL_RenderClear(m_Renderer);

	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_Renderer);

	SDL_RenderPresent(m_Renderer);
}



void Launcher::DrawHeader()
{
	const char* title = (const char*)ICON_FA_ROCKET "  Raekor";
	const float title_size = ImGui::GetFontSize() * 1.5f;
	const ImVec2 title_extent = ImGui::GetFont()->CalcTextSizeA(title_size, FLT_MAX, 0.0f, title);

	ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), title_size, ImGui::GetCursorScreenPos(), ImGui::GetColorU32(ImGuiCol_Text), title);
	ImGui::Dummy(title_extent);

	ImGui::TextDisabled("Pick a game and adjust startup settings, then press Launch or Enter to play.");

	ImGui::Spacing();
}



void Launcher::DrawGames()
{
	ImGui::SeparatorText("Games");

	if (m_Games.empty())
	{
		ImGui::TextDisabled("No games found, save a scene to %s to add one.", cGameScenesDirectory);
		ImGui::Spacing();
		return;
	}

	const ImGuiStyle& style = ImGui::GetStyle();
	const float tile_width = ImGui::GetFontSize() * 7.5f;
	const float right_edge = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;

	for (const auto& [index, game] : gEnumerate(m_Games))
	{
		ImGui::PushID(game.mSceneFile.string().c_str());

		if (index > 0 && ImGui::GetItemRectMax().x + style.ItemSpacing.x + tile_width <= right_edge)
			ImGui::SameLine();

		if (DrawGameTile(game, game.mSceneFile == m_SceneFile))
			SetSceneFile(game.mSceneFile);

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Play %s\n%s", game.mName.c_str(), game.mSceneFile.string().c_str());

		ImGui::PopID();
	}

	ImGui::Spacing();
}



bool Launcher::DrawGameTile(const GameEntry& inGame, bool inSelected)
{
	const char* icon = (const char*)ICON_FA_GAMEPAD;
	const char* name = inGame.mName.c_str();

	const ImVec2 tile_size = ImVec2(ImGui::GetFontSize() * 7.5f, ImGui::GetFontSize() * 4.5f);

	const ImVec2 tile_min = ImGui::GetCursorScreenPos();
	const ImVec2 tile_max = tile_min + tile_size;

	ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
	const bool clicked = ImGui::Selectable("##Tile", inSelected, ImGuiSelectableFlags_AllowDoubleClick, tile_size);
	ImGui::PopStyleVar();

	const bool double_clicked = clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	const ImGuiStyle& style = ImGui::GetStyle();

	const float rounding = glm::max(style.FrameRounding, 4.0f);
	const ImU32 border_color = ImGui::GetColorU32(inSelected ? ImGuiCol_ButtonActive : ImGuiCol_Border);

	draw_list->AddRect(tile_min, tile_max, border_color, rounding, 0, inSelected ? 2.0f : 1.0f);

	const float icon_size = ImGui::GetFontSize() * 1.6f;
	const ImVec2 icon_extent = ImGui::GetFont()->CalcTextSizeA(icon_size, FLT_MAX, 0.0f, icon);
	const ImVec2 name_extent = ImGui::CalcTextSize(name);

	const float content_height = icon_extent.y + style.ItemSpacing.y + name_extent.y;
	const float content_top = tile_min.y + ( tile_size.y - content_height ) * 0.5f;

	const ImU32 text_color = ImGui::GetColorU32(ImGuiCol_Text);

	draw_list->AddText(ImGui::GetFont(), icon_size, ImVec2(tile_min.x + ( tile_size.x - icon_extent.x ) * 0.5f, content_top), text_color, icon);
	draw_list->AddText(ImVec2(tile_min.x + ( tile_size.x - name_extent.x ) * 0.5f, content_top + icon_extent.y + style.ItemSpacing.y), text_color, name);

	if (double_clicked)
		Launch();

	return clicked;
}



void Launcher::DrawStartupSettings()
{
	ImGui::SeparatorText("Startup");

	const float label_width = ImGui::GetFontSize() * 5.0f;

	auto DrawLabel = [label_width](const char* inLabel)
	{
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(inLabel);
		ImGui::SameLine(label_width);
	};

	DrawLabel("Display");

	int display_count = 0;
	SDL_DisplayID* displays = SDL_GetDisplays(&display_count);

	const SDL_DisplayID current_display = SDL_GetDisplayForWindow(m_Window);
	const char* current_display_name = SDL_GetDisplayName(current_display);

	ImGui::SetNextItemWidth(-FLT_MIN);

	if (ImGui::BeginCombo("##Display", current_display_name ? current_display_name : "Unknown"))
	{
		for (int index = 0; index < display_count; index++)
		{
			SDL_Rect bounds = {};
			SDL_GetDisplayBounds(displays[index], &bounds);

			const char* display_name = SDL_GetDisplayName(displays[index]);
			const String label = std::format("{}  ({}x{})##{}", display_name ? display_name : "Unknown", bounds.w, bounds.h, displays[index]);

			if (ImGui::Selectable(label.c_str(), displays[index] == current_display))
				SetDisplay(displays[index]);
		}

		ImGui::EndCombo();
	}

	SDL_free(displays);

	ImGui::Spacing();
}



void Launcher::DrawConsoleVariables()
{
	ImGui::SeparatorText("Console Variables");

	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##Filter", (const char*)ICON_FA_SEARCH "  Search console variables..", &m_Filter);

	const float footer_height = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y * 2.0f + ImGui::GetStyle().SeparatorTextBorderSize;

	const ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchProp;

	if (!ImGui::BeginTable("Console Variables", 2, table_flags, ImVec2(0.0f, -footer_height)))
		return;

	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None, 0.6f);
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_None, 0.4f);
	ImGui::TableHeadersRow();

	const ImGuiTextFilter filter = ImGuiTextFilter(m_Filter.c_str());

	int visible_count = 0;

	for (const String& cvar_name : m_SortedCvarNames)
	{
		if (!filter.PassFilter(cvar_name.c_str()))
			continue;

		CVar& cvar = g_CVariables->GetCVar(cvar_name);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(cvar_name.c_str());

		ImGui::TableNextColumn();
		ImGui::PushID(cvar_name.c_str());
		ImGui::SetNextItemWidth(-FLT_MIN);

		switch (cvar.mType)
		{
			case CVAR_TYPE_INT:
			{
				if (sIsToggleCVar(cvar_name))
				{
					bool value = cvar.mIntValue != 0;

					if (ImGui::Checkbox("##Value", &value))
						cvar.mIntValue = int(value);
				}
				else
					ImGui::InputInt("##Value", &cvar.mIntValue, 0, 0);
			} break;

			case CVAR_TYPE_FLOAT:
				ImGui::InputFloat("##Value", &cvar.mFloatValue, 0.0f, 0.0f, "%.3f");
				break;

			case CVAR_TYPE_STRING:
				ImGui::InputText("##Value", &cvar.mStringValue);
				break;

			default: break;
		}

		ImGui::PopID();

		visible_count++;
	}

	if (visible_count == 0)
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextDisabled(m_SortedCvarNames.empty() ? "No console variables yet, they are created the first time the engine runs." : "No console variables match the search.");
	}

	ImGui::EndTable();
}



void Launcher::DrawFooter()
{
	ImGui::Separator();

	const ImGuiStyle& style = ImGui::GetStyle();
	const float button_width = ImGui::GetFontSize() * 7.0f;

	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + glm::max(ImGui::GetContentRegionAvail().x - button_width * 2.0f - style.ItemSpacing.x, 0.0f));

	if (ImGui::Button((const char*)ICON_FA_TIMES "  Quit", ImVec2(button_width, 0.0f)))
		m_Running = false;

	ImGui::SameLine();

	ImGui::BeginDisabled(m_SceneFile.empty());
	ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);

	if (ImGui::Button((const char*)ICON_FA_PLAY "  Launch", ImVec2(button_width, 0.0f)))
		Launch();

	ImGui::PopStyleColor();
	ImGui::EndDisabled();
}



void Launcher::SetDisplay(SDL_DisplayID inDisplay)
{
	m_ConfigSettings.mDisplayID = inDisplay;

	SDL_SetWindowPosition(m_Window, SDL_WINDOWPOS_CENTERED_DISPLAY(inDisplay), SDL_WINDOWPOS_CENTERED_DISPLAY(inDisplay));
}



void Launcher::SetSceneFile(const Path& inSceneFile)
{
	m_SceneFile = inSceneFile;

	g_CVariables->GetValue<String>(cLauncherGameCVar) = inSceneFile.generic_string();
}



void Launcher::Launch()
{
	if (m_SceneFile.empty())
		return;

	m_Launch = true;
	m_Running = false;
}



void Launcher::OnEvent(const SDL_Event& inEvent)
{
	ImGui_ImplSDL3_ProcessEvent(&inEvent);
}

} // raekor
