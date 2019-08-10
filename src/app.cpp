#include "pch.h"
#include "app.h"
#include "util.h"
#include "model.h"
#include "entry.h"
#include "camera.h"
#include "shader.h"
#include "framebuffer.h"
#include "PlatformContext.h"
#include "renderer.h"
#include "DXRenderer.h"
#include "DXShader.h"
#include "buffer.h"
#include "DXBuffer.h"
#include "DXFrameBuffer.h"

namespace Raekor {

void Application::serialize_settings(const std::string& filepath, bool write) {
    if (write) {
        std::ofstream os(filepath);
        cereal::JSONOutputArchive archive(os);
        serialize(archive);
    } else {
        std::ifstream is(filepath);
        cereal::JSONInputArchive archive(is);
        serialize(archive);
    }
}

void Raekor::Application::run() {
    auto context = Raekor::PlatformContext();
    std::cout << get_extension("C:\\Users\\LTSC2019\\InstallAnywhere\\myfile.jpg") << std::endl;

    // retrieve the application settings from the config file
    serialize_settings("config.json");

    m_assert(SDL_Init(SDL_INIT_VIDEO) == 0, "failed to init sdl");

    Uint32 wflags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED;
    Uint32 d3wflags = SDL_WINDOW_RESIZABLE |
        SDL_WINDOW_ALLOW_HIGHDPI;

    std::vector<SDL_Rect> displays;
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        displays.push_back(SDL_Rect());
        SDL_GetDisplayBounds(i, &displays.back());
    }

    // if our display setting is higher than the nr of displays we pick the default display
    auto index = display > displays.size() - 1 ? 0 : display;
    auto main_window = SDL_CreateWindow(name.c_str(),
        displays[index].x,
        displays[index].y,
        displays[index].w,
        displays[index].h,
        wflags);

    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImFont* pFont = io.Fonts->AddFontFromFileTTF(font.c_str(), 18.0f);
    if (!io.Fonts->Fonts.empty()) {
        io.FontDefault = io.Fonts->Fonts.back();
    }
    ImGui::StyleColorsDark();

    // initialize renderer(s)
    std::unique_ptr<GLRenderer> renderer;
    renderer.reset(new GLRenderer(main_window));

    // load our shaders and get the MVP handles
    auto simple_shader = Raekor::GLShader("shaders/simple.vert", "shaders/simple.frag");
    auto skybox_shader = Raekor::GLShader("shaders/skybox.vert", "shaders/skybox.frag");

    // create a Camera we can use to move around our scene
    Raekor::Camera camera(glm::vec3(0, 0, 5), 45.0f);

    // Setup our scene TODO: create a class for this that's easy to serialize
    std::vector<Raekor::TexturedModel> scene;
    int active_model = 0;

    // persistent imgui variable values
    std::string mesh_file = "";
    std::string texture_file = "";
    auto active_skybox = skyboxes.begin();

    // frame buffer for the renderer
    std::unique_ptr<Raekor::FrameBuffer> frame_buffer;
    frame_buffer.reset(Raekor::FrameBuffer::construct(glm::vec2(1280, 720)));

    // load a cube mesh and a texture image to use as skybox
    std::unique_ptr<Raekor::GLTextureCube> skybox_texture;
    skybox_texture.reset(new Raekor::GLTextureCube(active_skybox->second));
    std::unique_ptr<Raekor::Mesh> skycube;
    skycube.reset(new Raekor::Mesh("resources/models/testcube.obj"));

    SDL_SetWindowInputFocus(main_window);

    //main application loop
    for (;;) {
        //handle sdl and imgui events
        handle_sdl_gui_events({ main_window }, camera);

        // clear opengl window background (blue)
        renderer->Clear(glm::vec4(0.22f, 0.32f, 0.42f, 1.0f));

        // match our GL viewport with the framebuffer's size and clear it
        auto fsize = frame_buffer->get_size();
        glViewport(0, 0, (GLsizei)fsize.x, (GLsizei)fsize.y);
        frame_buffer->bind();
        renderer->Clear(glm::vec4(0.0f, 0.22f, 0.22f, 1.0f));

        // bind the skybox shader, update the camera WITHOUT translation (important for skybox)
        // upload the MVP, bind the skybox texture and render the entire thing
        glDepthMask(GL_FALSE);
        skybox_shader.bind();
        camera.update();
        skybox_shader["MVP"] = glm::value_ptr(camera.get_mvpLH());
        // bind our texture
        skybox_texture->bind();
        skycube->bind();
        // draw indexed
        renderer->DrawIndexed(skycube->get_index_buffer()->get_count());
        // unbind the texture
        skybox_texture->unbind();
        glDepthMask(GL_TRUE);


        // bind the simple shader and draw textured model in our scene
        simple_shader.bind();
        for (auto& m : scene) {
            camera.update(m.get_mesh()->get_transform());
            simple_shader["MVP"] = glm::value_ptr(camera.get_mvpLH());
            m.bind();
            renderer->DrawIndexed(m.get_mesh()->get_index_buffer()->get_count());

        }
        simple_shader.unbind();
        frame_buffer->unbind();

        //get new frame for render API, sdl and imgui
        renderer->ImGui_NewFrame(main_window);

        // draw the top level bar that contains stuff like "File" and "Edit"
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Close", "CTRL+S")) {
                    //clean up imgui
                    ImGui_ImplOpenGL3_Shutdown();
                    ImGui_ImplSDL2_Shutdown();
                    ImGui::DestroyContext();

                    //clean up SDL
                    SDL_DestroyWindow(main_window);
                    SDL_Quit();
                    return;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // renderer viewport
        ImGui::SetNextWindowContentSize(ImVec2(fsize.x, fsize.y));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Renderer", NULL, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Image((void*)static_cast<size_t>(frame_buffer->get_frame()), ImVec2(fsize.x, fsize.y), { 0,1 }, { 1,0 });
        ImGui::End();
        ImGui::PopStyleVar();

        // scene panel
        ImGui::SetNextWindowSize(ImVec2(760, 260), ImGuiCond_Once);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 770, io.DisplaySize.y - 550), ImGuiCond_Once);
        ImGui::Begin("Scene");
        if (ImGui::BeginCombo("Skybox", active_skybox->first.c_str())) {
            for (auto it = skyboxes.begin(); it != skyboxes.end(); it++) {
                bool selected = (it == active_skybox);
                if (ImGui::Selectable(it->first.c_str(), selected)) {
                    active_skybox = it;
                    skybox_texture.reset(new Raekor::GLTextureCube(active_skybox->second));
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        // combo box used to select the mesh you're trying to edit
        if (ImGui::BeginCombo("", mesh_file.c_str())) {
            for (int i = 0; i < scene.size(); i++) {
                bool selected = (i == active_model);

                //hacky way to give the selectable a unique ID
                auto name = scene[i].get_mesh()->get_mesh_path();
                name = name + "##" + std::to_string(i);

                if (ImGui::Selectable(name.c_str(), selected)) {
                    active_model = i;
                    mesh_file = Raekor::get_extension(scene[active_model].get_mesh()->get_mesh_path());
                    if (scene[active_model].get_texture() != nullptr) {
                        texture_file = Raekor::get_extension(scene[active_model].get_texture()->get_path());
                    }
                    else texture_file = "";
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        // plus button that opens a file dialog window using the platform context to ask what model file you want to load in
        ImGui::SameLine();
        if (ImGui::Button("+")) {
            std::string path = context.open_file_dialog({ ".obj" });
            if (!path.empty()) {
                scene.push_back(Raekor::TexturedModel(path, ""));
                active_model = (int)scene.size() - 1;
                mesh_file = Raekor::get_extension(scene[active_model].get_mesh()->get_mesh_path());
                texture_file = "";
            }
        }
        // minus button that removes the active model
        ImGui::SameLine();
        if (ImGui::Button("-") && !scene.empty()) {
            scene.erase(scene.begin() + active_model);
            active_model = 0;
            // if we just deleted the last model, empty the strings
            if (scene.empty()) {
                mesh_file = "";
                texture_file = "";
            }
            else {
                mesh_file = Raekor::get_extension(scene[active_model].get_mesh()->get_mesh_path());
                if (scene[active_model].get_texture() != nullptr) {
                    texture_file = Raekor::get_extension(scene[active_model].get_texture()->get_path());
                }
                else texture_file = "";
            }
        }
        // toggle button for openGl vsync
        static bool is_vsync = true;
        if (ImGui::RadioButton("USE VSYNC", is_vsync)) {
            is_vsync = !is_vsync;
            SDL_GL_SetSwapInterval(is_vsync);
        }
        ImGui::End();

        // if we have at least 1 model in our scene we draw the panel that controls the models' variables
        if (!scene.empty()) {
            //start drawing a new imgui window. TODO: make this into a reusable component
            ImGui::SetNextWindowSize(ImVec2(760, 260), ImGuiCond_Once);
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 770, io.DisplaySize.y - 270), ImGuiCond_Once);
            ImGui::Begin("Object Properties");

            // opens a file dialog to select a new mesh for the model
            ImGui::Text("Mesh");
            ImGui::Text(mesh_file.c_str(), NULL);
            ImGui::SameLine();
            if (ImGui::Button("...##Mesh")) {
                std::string path = context.open_file_dialog({ ".obj" });
                if (!path.empty()) {
                    scene[active_model].set_mesh(path);
                    mesh_file = Raekor::get_extension(scene[active_model].get_mesh()->get_mesh_path());

                }
            }
            ImGui::Separator();
            // opens a file dialog to select a new texture for the model
            ImGui::Text("Texture");
            ImGui::Text(texture_file.c_str(), NULL);
            ImGui::SameLine();
            if (ImGui::Button("...##Texture")) {
                std::string path = context.open_file_dialog({ ".png" });
                if (!path.empty()) {
                    scene[active_model].set_texture(path);
                    texture_file = Raekor::get_extension(scene[active_model].get_texture()->get_path());
                }
            }

            if (ImGui::DragFloat3("Scale", scene[active_model].get_mesh()->scale_ptr(), 0.01f, 0.0f, 10.0f)) {
                scene[active_model].get_mesh()->recalc_transform();
            }
            if (ImGui::DragFloat3("Position", scene[active_model].get_mesh()->pos_ptr(), 0.01f, -100.0f, 100.0f)) {
                scene[active_model].get_mesh()->recalc_transform();
            }
            if (ImGui::DragFloat3("Rotation", scene[active_model].get_mesh()->rotation_ptr(), 0.01f, (float)(-M_PI), (float)(M_PI))) {
                scene[active_model].get_mesh()->recalc_transform();
            }

            // resets the model's transformation
            if (ImGui::Button("Reset")) {
                scene[active_model].get_mesh()->reset_transform();
            }
            ImGui::End();
        }
        renderer->ImGui_Render();

        int w, h;
        SDL_GetWindowSize(main_window, &w, &h);
        glViewport(0, 0, w, h);

        SDL_GL_SwapWindow(main_window);
    }
}

void Application::run_dx() {
    auto context = Raekor::PlatformContext();
    Renderer::set_activeAPI(RenderAPI::DIRECTX11);

    // retrieve the application settings from the config file
    serialize_settings("config.json");

    m_assert(SDL_Init(SDL_INIT_VIDEO) == 0, "failed to init sdl");

    Uint32 wflags = SDL_WINDOW_RESIZABLE |
        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED;

    std::vector<SDL_Rect> displays;
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        displays.push_back(SDL_Rect());
        SDL_GetDisplayBounds(i, &displays.back());
    }

    // if our display setting is higher than the nr of displays we pick the default display
    auto index = display > displays.size() - 1 ? 0 : display;
    auto directxwindow = SDL_CreateWindow(name.c_str(),
        displays[index].x,
        displays[index].y,
        displays[index].w,
        displays[index].h,
        wflags);

    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version);
    SDL_GetWindowWMInfo(directxwindow, &wminfo);
    auto dx_hwnd = wminfo.info.win.window;

    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImFont* pFont = io.Fonts->AddFontFromFileTTF(font.c_str(), 18.0f);
    if (!io.Fonts->Fonts.empty()) {
        io.FontDefault = io.Fonts->Fonts.back();
    }
    ImGui::StyleColorsDark();

    std::unique_ptr<DXRenderer> dxr;
    dxr.reset(new DXRenderer(directxwindow));

    std::unique_ptr<DXShader> dx_shader;
    dx_shader.reset(new DXShader("shaders/simple_vertex.cso", "shaders/simple_pixel.cso"));

    // com pointers to direct3d11 variables
    Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
    com_ptr<ID3D11SamplerState> sampler_state;
    com_ptr<ID3D11ShaderResourceView> texture;
    
    auto hr = DirectX::CreateWICTextureFromFile(D3D.device.Get(), L"resources\\textures\\grass block.png", nullptr, texture.GetAddressOf());
    m_assert(SUCCEEDED(hr), "failed to load texture");

    D3D11_SAMPLER_DESC samp_desc;
    memset(&samp_desc, 0, sizeof(D3D11_SAMPLER_DESC));
    samp_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samp_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samp_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samp_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samp_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samp_desc.MinLOD = 0;
    samp_desc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = D3D.device->CreateSamplerState(&samp_desc, sampler_state.GetAddressOf());
    m_assert(SUCCEEDED(hr), "failed to crreate sampler state");


    // struct that describes and matches what the hlsl vertex shader expects
    struct cb_vs {
        glm::mat4 MVP;
    };

    // fill out the constant buffer description for our MVP struct
    D3D11_BUFFER_DESC cbdesc;
    cbdesc.Usage = D3D11_USAGE_DYNAMIC;
    cbdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbdesc.MiscFlags = 0;
    cbdesc.ByteWidth = static_cast<UINT>(sizeof(cb_vs) + (16 - (sizeof(cb_vs) % 16)));
    cbdesc.StructureByteStride = 0;
    
    // test cube for directx rendering
    std::unique_ptr<Raekor::Mesh> mcube;
    mcube.reset(new Raekor::Mesh("resources/models/minecraft_block.obj"));

    std::unique_ptr<DXVertexBuffer> dxvb;
    dxvb.reset(new DXVertexBuffer(mcube->vertexes));
    std::unique_ptr<DXIndexBuffer> dxib;
    dxib.reset(new DXIndexBuffer(mcube->indexes));

    // create a Camera we can use to move around our scene
    Raekor::Camera camera(glm::vec3(0, 0, 5), 45.0f);

    // persistent imgui variable values
    std::string mesh_file = "";
    std::string texture_file = "";
    auto active_skybox = skyboxes.begin();

    std::unique_ptr<DXFrameBuffer> dxfb;
    dxfb.reset(new DXFrameBuffer({ 1280, 720 }));
    auto fsize = dxfb->get_size();

    SDL_SetWindowInputFocus(directxwindow);

    //main application loop
    for (;;) {
        //handle sdl and imgui events
        handle_sdl_gui_events({ directxwindow}, camera);

        dxr->Clear({ 0.22f, 0.32f, 0.42f, 1.0f });
        dxfb->bind();
        dxr->Clear({ 0.0f, 0.32f, 0.42f, 1.0f });
        // set the input layout, topology, rasterizer state and bind our vertex and pixel shader
        // TODO: right now it sets all these things in the vertex buffer bind call, this seems like a weird design choice but works for now
        dx_shader->bind();

        // set our constant buffer data containing the MVP of our mesh/model
        cb_vs data;
        auto cube_rotation = mcube->rotation_ptr();
        *cube_rotation += 0.01f;
        mcube->recalc_transform();
        data.MVP = camera.get_mvpRH(mcube->get_transform());

        // create a directx resource for our MVP data
        D3D11_SUBRESOURCE_DATA cbdata;
        cbdata.pSysMem = &data;
        cbdata.SysMemPitch = 0;
        cbdata.SysMemSlicePitch = 0;
        auto ret = D3D.device->CreateBuffer(&cbdesc, &cbdata, constant_buffer.GetAddressOf());
        if (FAILED(ret)) m_assert(false, "failed to create constant buffer");

        D3D.context->PSSetShaderResources(0, 1, texture.GetAddressOf());
        D3D.context->PSSetSamplers(0, 1, sampler_state.GetAddressOf());

        // bind our constant, vertex and index buffers
        D3D.context->VSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
        dxvb->bind();
        dxib->bind();

        // draw the indexed vertices and swap the backbuffer to front
        dxr->DrawIndexed(dxib->get_count());

        dxfb->unbind();

        //get new frame for render API, sdl and imgui
        dxr->ImGui_NewFrame(directxwindow);

        // draw the top level bar that contains stuff like "File" and "Edit"
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Close", "CTRL+S")) {
                    //clean up imgui
                    ImGui_ImplOpenGL3_Shutdown();
                    ImGui_ImplSDL2_Shutdown();
                    ImGui::DestroyContext();

                    //clean up SDL
                    SDL_DestroyWindow(directxwindow);
                    SDL_Quit();
                    return;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // renderer viewport
        ImGui::SetNextWindowContentSize(ImVec2(fsize.x, fsize.y));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Renderer", NULL, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Image(dxfb->get_data(), ImVec2(fsize.x, fsize.y));
        ImGui::End();
        ImGui::PopStyleVar();

        dxfb->unbind();

        // scene panel
        ImGui::SetNextWindowSize(ImVec2(760, 260), ImGuiCond_Once);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 770, io.DisplaySize.y - 550), ImGuiCond_Once);
        ImGui::Begin("Scene");
        if (ImGui::BeginCombo("Skybox", active_skybox->first.c_str())) {
            for (auto it = skyboxes.begin(); it != skyboxes.end(); it++) {
                bool selected = (it == active_skybox);
                if (ImGui::Selectable(it->first.c_str(), selected)) {
                    active_skybox = it;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        // toggle button for openGl vsync
        static bool is_vsync = true;
        if (ImGui::RadioButton("USE VSYNC", is_vsync)) {
            is_vsync = !is_vsync;
            SDL_GL_SetSwapInterval(is_vsync);
        }
        ImGui::End();

            //start drawing a new imgui window. TODO: make this into a reusable component
            ImGui::SetNextWindowSize(ImVec2(760, 260), ImGuiCond_Once);
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 770, io.DisplaySize.y - 270), ImGuiCond_Once);
            ImGui::Begin("Object Properties");

            if (ImGui::DragFloat3("Scale", mcube->scale_ptr(), 0.01f, 0.0f, 10.0f)) {
                mcube->recalc_transform();
            }
            if (ImGui::DragFloat3("Position", mcube->pos_ptr(), 0.01f, -100.0f, 100.0f)) {
                mcube->recalc_transform();
            }
            if (ImGui::DragFloat3("Rotation", mcube->rotation_ptr(), 0.01f, (float)(-M_PI), (float)(M_PI))) {
                mcube->recalc_transform();
            }

            // resets the model's transformation
            if (ImGui::Button("Reset")) {
                mcube->reset_transform();
            }
            ImGui::End();
            dxr->ImGui_Render();
            D3D.swap_chain->Present(1, NULL);
        }
    }

} // namespace Raekor