#include "pch.h"
#include "compiler.h"
#include "gui.h"
#include "archive.h"
#include "timer.h"
#include "async.h"
#include "assimp.h"
#include "components.h"

#define WIN_TRAY_MESSAGE WM_USER + 1
#define IDI_ICON1 102


namespace Raekor {

CompilerApp::CompilerApp(WindowFlags inFlags) : Application(inFlags | WindowFlag::RESIZE) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().IniFilename = "";

    if (!m_Settings.mFontFile.empty())
        GUI::SetFont(m_Settings.mFontFile);
    
    GUI::SetTheme();
    ImGui::GetStyle().ScaleAllSizes(1.33333333f);

    m_Renderer = SDL_CreateRenderer(m_Window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    
    SDL_RendererInfo renderer_info;
    SDL_GetRendererInfo(m_Renderer, &renderer_info);
    std::cout << "Created SDL_Renderer with name: \"" << renderer_info.name << "\"\n";

    ImGui_ImplSDL2_InitForSDLRenderer(m_Window, m_Renderer);
    ImGui_ImplSDLRenderer_Init(m_Renderer);
    SDL_SetWindowTitle(m_Window, "Raekor Compiler App");

    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version);
    SDL_GetWindowWMInfo(m_Window, &wminfo);

    SDL_GL_SetSwapInterval(1);

    // Add the system tray icon
    NOTIFYICONDATA nid = { sizeof(NOTIFYICONDATA) };
    nid.hWnd = wminfo.info.win.window;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WIN_TRAY_MESSAGE;
    nid.hIcon = (HICON)GetClassLongPtr(wminfo.info.win.window, -14);
    strcpy(nid.szTip, "Raekor Compiler App");

    Shell_NotifyIcon(NIM_ADD, &nid);

    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);

#ifdef NDEBUG
    // hide the console window
    ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif

    SDL_SetWindowTitle(m_Window, "Raekor Asset Compiler");

    for (const auto& file : FileSystem::recursive_directory_iterator("assets")) {
        if (!file.is_regular_file())
            continue;

        const auto path = file.path();
        const auto extension = path.extension();

        const auto new_extension = GetCacheFileExtension(file);
        if (new_extension == ASSET_TYPE_NONE)
            continue;

        //if (m_Files.size() > 65)
            //return;

        auto& file_entry = m_Files.emplace_back(file);
        file_entry.ReadMetadata();
    }

    g_ThreadPool.SetActiveThreadCount(g_ThreadPool.GetThreadCount() / 4);

    stbi_set_flip_vertically_on_load(true);
}



CompilerApp::~CompilerApp() {
    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version);
    SDL_GetWindowWMInfo(m_Window, &wminfo);

    NOTIFYICONDATA nid = { sizeof(NOTIFYICONDATA) };
    nid.hWnd = wminfo.info.win.window;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER + 1;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    strcpy(nid.szTip, "Raekor Compiler App");
    Shell_NotifyIcon(NIM_DELETE, &nid);

    ImGui_ImplSDL2_Shutdown();
    ImGui_ImplSDLRenderer_Shutdown();
    SDL_DestroyRenderer(m_Renderer);
    ImGui::DestroyContext();
}



void CompilerApp::OnUpdate(float inDeltaTime) {
    // SDL_SetWindowTitle(m_Window, std::string(std::to_string(Timer::sToMilliseconds(inDeltaTime)) + " ms.").c_str());

    ImGui_ImplSDL2_NewFrame();
    ImGui_ImplSDLRenderer_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) {
        ImGui::Text(reinterpret_cast<const char*>(ICON_FA_ADDRESS_BOOK));

        if (ImGui::BeginMenu("File")) {

            if (ImGui::MenuItem("Exit"))
                m_Running = false;

            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }

    ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetCursorPos().y - ImGui::GetFontSize() * 0.5), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    const auto frame_padding = ImGui::GetStyle().FramePadding;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(frame_padding.x, frame_padding.y * 0.5));

    auto window_flags = ImGuiWindowFlags_NoScrollWithMouse | 
                        ImGuiWindowFlags_NoScrollbar | 
                        ImGuiWindowFlags_NoTitleBar | 
                        ImGuiWindowFlags_NoResize | 
                        ImGuiWindowFlags_NoCollapse | 
                        ImGuiWindowFlags_NoSavedSettings;

    bool open = true;
    ImGui::Begin("##Compiler", &open, window_flags);

    auto compile_scenes = m_CompileScenes.load();
    if (ImGui::Checkbox("Convert Models", &compile_scenes))
        m_CompileScenes.store(compile_scenes);

    ImGui::SameLine();

    auto compile_textures = m_CompileTextures.load();
    if (ImGui::Checkbox("Convert Textures", &compile_textures))
        m_CompileTextures.store(compile_textures);

    auto flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable;
    ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.15, 0.15, 0.15, 1.0));
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.20, 0.20, 0.20, 1.0));

    if (ImGui::BeginTable("Assets", 4, flags)) {
        ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
        ImGui::TableSetupColumn("Asset File Path");
        // ImGui::TableSetupColumn("Asset Type");
        ImGui::TableSetupColumn("Cached File Path");
        ImGui::TableSetupColumn("FNV-1a File Hash");
        ImGui::TableSetupColumn("File Modified Time");
        ImGui::TableHeadersRow();

        for (const auto& [index, file] : gEnumerate(m_Files)) {
            // Sort our data if sort specs have been changed!
            if (ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs()) {
                if (sorts_specs->SpecsDirty) {
                    for (int n = 0; n < sorts_specs->SpecsCount; n++) {
                        const ImGuiTableColumnSortSpecs* sort_spec = &sorts_specs->Specs[n];
                        switch (sort_spec->ColumnIndex) {
                            case 0: {
                                if (sort_spec->SortDirection == ImGuiSortDirection_Ascending)
                                    std::sort(m_Files.begin(), m_Files.end(), [](const auto& lhs, const auto& rhs) { return lhs.mAssetPath < rhs.mAssetPath; });
                                else
                                    std::sort(m_Files.begin(), m_Files.end(), [](const auto& lhs, const auto& rhs) { return lhs.mAssetPath > rhs.mAssetPath; });
                                break;
                            };
                            case 1: {
                                if (sort_spec->SortDirection == ImGuiSortDirection_Ascending)
                                    std::sort(m_Files.begin(), m_Files.end(), [](const auto& lhs, const auto& rhs) { return lhs.mCachePath < rhs.mCachePath; });
                                else
                                    std::sort(m_Files.begin(), m_Files.end(), [](const auto& lhs, const auto& rhs) { return lhs.mCachePath > rhs.mCachePath; });
                                break;
                            };
                            case 2: {
                                if (sort_spec->SortDirection == ImGuiSortDirection_Ascending)
                                    std::sort(m_Files.begin(), m_Files.end(), [](const auto& lhs, const auto& rhs) { return lhs.mFileHash > rhs.mFileHash; });
                                else
                                    std::sort(m_Files.begin(), m_Files.end(), [](const auto& lhs, const auto& rhs) { return lhs.mFileHash < rhs.mFileHash; });
                                break;
                            };
                            case 3: {
                                if (sort_spec->SortDirection == ImGuiSortDirection_Ascending)
                                    std::sort(m_Files.begin(), m_Files.end(), [](const auto& lhs, const auto& rhs) { return lhs.mWriteTime < rhs.mWriteTime; });
                                else
                                    std::sort(m_Files.begin(), m_Files.end(), [](const auto& lhs, const auto& rhs) { return lhs.mWriteTime > rhs.mWriteTime; });
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

            if (ImGui::BeginPopupContextItem()) {
                m_SelectedIndex = index;

                if (ImGui::MenuItem("Open..")) {
                    if (FileSystem::exists(file.mCachePath))
                        ShellExecute(NULL, "open", file.mCachePath.c_str(), NULL, NULL, SW_RESTORE);
                    else
                        ShellExecute(NULL, "open", file.mAssetPath.c_str(), NULL, NULL, SW_RESTORE);
                }

                if (ImGui::MenuItem("Delete", "DELETE")) {
                    const auto& file = m_Files[m_SelectedIndex];

                    auto error_code = std::error_code();
                    if (FileSystem::exists(file.mCachePath, error_code))
                        FileSystem::remove(file.mCachePath);
                }

                if (ImGui::MenuItem("Open Containing Folder..")) {
                    auto& filepath = FileSystem::exists(file.mCachePath) ? file.mCachePath : file.mAssetPath;
                    auto folder = Path(filepath).parent_path().string();
                    ShellExecute(NULL, "open", folder.c_str(), NULL, NULL, SW_RESTORE);
                }

                ImGui::EndPopup();
            }
            else if (ImGui::IsItemHovered())
            {
                if (ImGui::IsMouseDoubleClicked(0)) {
                    if (FileSystem::exists(file.mCachePath))
                        ShellExecute(NULL, "open", file.mCachePath.c_str(), NULL, NULL, SW_RESTORE);
                    else
                        ShellExecute(NULL, "open", file.mAssetPath.c_str(), NULL, NULL, SW_RESTORE);
                }
                else if (ImGui::IsMouseClicked(0))
                    m_SelectedIndex = index;    
            }

            ImGui::PopStyleVar();

            ImGui::SameLine();

            auto error_code = std::error_code();
            if (FileSystem::exists(file.mCachePath, error_code))
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));
            else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
            }
            
            ImGui::Text(reinterpret_cast<const char*>(ICON_FA_CLOUD));

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

    ImGui::PopStyleColor();
    ImGui::PopStyleColor();

    ImGui::End(); // ##Window
    ImGui::PopStyleVar(1); // ImGuiStyleVar_WindowRounding 0.0f
    ImGui::PopStyleVar(1); // ImGuiStyleVar_WindowRounding 0.0f

    ImGui::ShowStyleEditor();

    ImGui::EndFrame();
    ImGui::Render();

    SDL_SetRenderDrawColor(m_Renderer, 0, 0, 0, 0);
    SDL_RenderClear(m_Renderer);

    ImGui::Render();
    ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());

    SDL_RenderPresent(m_Renderer);

    for (auto [index, file] : gEnumerate(m_Files)) {
        if (FileSystem::exists(file.mCachePath) || m_FilesInFlight.contains(index))
            continue;

        if (file.mAssetType == ASSET_TYPE_IMAGE && m_CompileTextures) {
            m_FilesInFlight.insert(index);

           g_ThreadPool.QueueJob([this, index, &file]() {
                auto asset_path = TextureAsset::sConvert(file.mAssetPath);
                FileSystem::create_directories(Path(file.mCachePath).parent_path());
                FileSystem::copy_file(asset_path, file.mCachePath);

                file.UpdateWriteTime();

                std::scoped_lock lock(m_FilesInFlightMutex);
                m_FilesInFlight.erase(index);
            });
        }
        else if (file.mAssetType == ASSET_TYPE_SCENE && m_CompileScenes) {
            m_FilesInFlight.insert(index);

            //g_ThreadPool.QueueJob([this, index, &file]() {
                auto assets = Assets();
                auto scene = Scene(nullptr); // nullptr, dont need a renderer
                auto importer = AssimpImporter(scene, nullptr); // nullptr, dont need a renderer
                importer.LoadFromFile(file.mAssetPath, nullptr); // nullptr, don't need assets

                const auto file_dir = importer.GetDirectory().string();

               // patch materials, ugh, bleh, todo: fixme
               if (const auto ai_scene = importer.GetAiScene()) {
                   for (auto [index, ai_material] : gEnumerate(Slice(ai_scene->mMaterials, ai_scene->mNumMaterials))) {
                       aiString albedoFile, normalmapFile, metalroughFile;
                       ai_material->GetTexture(aiTextureType_DIFFUSE, 0, &albedoFile);
                       ai_material->GetTexture(aiTextureType_NORMALS, 0, &normalmapFile);
                       ai_material->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &metalroughFile);

                       auto& material = scene.get<Material>(importer.GetMaterials()[index]);

                       if (albedoFile.length) {
                           material.albedoFile = file_dir + albedoFile.C_Str();
                            material.albedoFile = FileSystem::relative(material.albedoFile).replace_extension(".dds").string().replace(0, 6, "cached");
                       }

                       if (normalmapFile.length) {
                           material.normalFile = file_dir + normalmapFile.C_Str();
                            material.normalFile = FileSystem::relative(material.normalFile).replace_extension(".dds").string().replace(0, 6, "cached");
                       }

                       if (metalroughFile.length) {
                           material.metalroughFile = file_dir + metalroughFile.C_Str();
                           material.metalroughFile = FileSystem::relative(material.metalroughFile).replace_extension(".dds").string().replace(0, 6, "cached");
                       }

                    }
               }

               FileSystem::create_directories(Path(file.mCachePath).parent_path());
               scene.SaveToFile(assets, file.mCachePath);

               std::scoped_lock lock(m_FilesInFlightMutex);
               m_FilesInFlight.erase(index);
            //});
        }
    }
}



void CompilerApp::OnEvent(const SDL_Event& inEvent) {
    ImGui_ImplSDL2_ProcessEvent(&inEvent);

    if (inEvent.type == SDL_WINDOWEVENT && inEvent.window.event == SDL_WINDOWEVENT_CLOSE)
            m_WasClosed = true;

    if (inEvent.type == SDL_WINDOWEVENT && inEvent.window.event == SDL_WINDOWEVENT_MINIMIZED)
        SDL_HideWindow(m_Window);

    if (inEvent.type == SDL_SYSWMEVENT) {
        auto& win_msg = inEvent.syswm.msg->msg.win;

        switch (win_msg.msg)
        {
        case WIN_TRAY_MESSAGE:
            switch (win_msg.lParam)
            {
            case WM_LBUTTONDBLCLK: {
                SDL_ShowWindow(m_Window);
                SDL_RestoreWindow(m_Window);
                SDL_RaiseWindow(m_Window);  

                SDL_SysWMinfo wminfo;
                SDL_VERSION(&wminfo.version);
                SDL_GetWindowWMInfo(m_Window, &wminfo);

                ShowWindow(wminfo.info.win.window, SW_RESTORE);
                break;
            }
            case WM_RBUTTONDOWN:
            case WM_CONTEXTMENU:
                break;
                //ShowContextMenu(hWnd);
            }
            break;
        }
    }


    if (inEvent.type == SDL_KEYDOWN && !inEvent.key.repeat) {
        switch (inEvent.key.keysym.sym) {
        case SDLK_DELETE: {
            if (m_SelectedIndex > 0 && m_SelectedIndex < m_Files.size()) {
                const auto& file = m_Files[m_SelectedIndex];

                auto error_code = std::error_code();
                if (FileSystem::exists(file.mCachePath, error_code))
                    FileSystem::remove(file.mCachePath);
            }

        } break;
        }
    }

}

}