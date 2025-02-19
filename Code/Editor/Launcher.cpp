#include "pch.h"
#include "Launcher.h"
#include "gui.h"

namespace RK {

SDL_Image::~SDL_Image()
{
	if (m_Texture)
		SDL_DestroyTexture(m_Texture);
	if (m_Surface)
		SDL_DestroySurface(m_Surface);
	if (m_PixelData)
		stbi_image_free(m_PixelData);
}



bool SDL_Image::Load(SDL_Renderer* inRenderer, const Path& inPath)
{
	int width = 0, height = 0, ch = 0;
	const String filepath = inPath.string();
	m_PixelData = stbi_load(filepath.c_str(), &width, &height, &ch, 0);

	if (!m_PixelData)
		return false;

	const int pitch = ( ( width * ch ) + 3 ) & ~3;
	m_Surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA8888, m_PixelData, pitch);

	if (!m_Surface)
	{
		stbi_image_free(m_PixelData);
		return false;
	}

	assert(inRenderer);
	m_Texture = SDL_CreateTextureFromSurface(inRenderer, m_Surface);

	if (!m_Texture)
		return false;

	return true;
}



Launcher::Launcher() : Application(WindowFlag::HIDDEN)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui::GetIO().IniFilename = "";

	// hide the console window
	if (!IsDebuggerPresent())
		ShowWindow(GetConsoleWindow(), SW_HIDE);

	if (!m_ConfigSettings.mFontFile.empty())
		GUI::SetFont(m_ConfigSettings.mFontFile.string());
	GUI::SetDarkTheme();

	m_Renderer = SDL_CreateRenderer(m_Window, NULL);
	std::cout << "Created SDL_Renderer with name: \"" << SDL_GetRendererName(m_Renderer)<< "\"\n";

	ImGui_ImplSDL3_InitForSDLRenderer(m_Window, m_Renderer);
	ImGui_ImplSDLRenderer3_Init(m_Renderer);
	SDL_SetWindowTitle(m_Window, "Launcher");
	SDL_SetWindowMinimumSize(m_Window, 420, 90);

	for (const auto& [cvar, value] : g_CVariables->GetCVars())
		m_SortedCvarNames.insert(cvar);

	m_NrOfRows = m_SortedCvarNames.size() / 2;

	//if (!m_BgImage.Load(m_Renderer, "assets/system/doom.jpg"))
	//    assert(false);
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

	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

	if (m_BgImage.IsLoaded())
		window_flags |= ImGuiWindowFlags_NoBackground;

	ImGui::Begin("##launcher", (bool*)1, window_flags);
	ImGui::BeginTable("Configuration Settings", 2, ImGuiTableFlags_SizingFixedFit);

	uint32_t index = 0u;
	int cNrOfColumns = 2;

	for (const String& cvar_name : m_SortedCvarNames)
	{
		CVar& cvar = g_CVariables->GetCVar(cvar_name);

		switch (cvar.mType)
		{
			case CVAR_TYPE_INT:
			{
				bool value = cvar.mIntValue;
				String string = "##" + cvar_name;

				if (ImGui::Checkbox(string.c_str(), &value))
					cvar = CVar(int(value));

				ImGui::SameLine();
				ImGui::Text(cvar_name.c_str());

				// ImGui::TableNextRow();

				if (index % m_NrOfRows == 0)
					ImGui::TableNextColumn();

				index++;
			} break;

			default: break;
		}
	}

	m_NrOfRows = index / 2;

	ImGui::EndTable();

	ImGui::NewLine();
	ImGui::NewLine();

	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10);
	if (ImGui::Button("Launch", ImVec2(-10, 20)))
	{
		m_Running = false;
	}

	ImVec2 empty_window_space = ImGui::GetContentRegionAvail();

	ImGui::End(); // ##Window
	ImGui::PopStyleVar(1); // ImGuiStyleVar_WindowRounding 0.0f

	ImGui::EndFrame();
	ImGui::Render();

	SDL_SetRenderDrawColor(m_Renderer, 0, 0, 0, 0);
	SDL_RenderClear(m_Renderer);

	if (m_BgImage.IsLoaded())
		SDL_RenderTexture(m_Renderer, m_BgImage.GetTexture(), NULL, NULL);

	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_Renderer);

	SDL_RenderPresent(m_Renderer);

	// This part will resize the SDL window to exactly fit the ImGui content
	if (( empty_window_space.x > 0 || empty_window_space.y > 0 ) && m_ResizeCounter <= 2)
	{
		int w = 0, h = 0;
		SDL_GetWindowSize(m_Window, &w, &h);
		SDL_SetWindowSize(m_Window, w - empty_window_space.x, h - empty_window_space.y);

		// At this point the window should be perfectly fitted to the ImGui content, the window was created with HIDDEN, so now we can SHOW it
		SDL_SetWindowPosition(m_Window, SDL_WINDOWPOS_CENTERED_DISPLAY(m_ConfigSettings.mDisplayID),
										SDL_WINDOWPOS_CENTERED_DISPLAY(m_ConfigSettings.mDisplayID));
		SDL_ShowWindow(m_Window);

		m_ResizeCounter++;
	}
}



void Launcher::OnEvent(const SDL_Event& inEvent)
{
	ImGui_ImplSDL3_ProcessEvent(&inEvent);

	if (inEvent.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
		m_WasClosed = true;
}

} // raekor
