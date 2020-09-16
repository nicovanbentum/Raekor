#include "pch.h"
#include "app.h"
#include "ecs.h"
#include "util.h"
#include "scene.h"
#include "mesh.h"
#include "entry.h"
#include "serial.h"
#include "camera.h"
#include "shader.h"
#include "script.h"
#include "framebuffer.h"
#include "platform/OS.h"
#include "renderer.h"
#include "buffer.h"
#include "timer.h"
#include "renderpass.h"
#include "gui.h"

namespace Raekor {

void Editor::serializeSettings(const std::string& filepath, bool write) {
    if (write) {
        std::ofstream os(filepath);
        cereal::JSONOutputArchive archive(os);
        serialize(archive);
    }
    else {
        std::ifstream is(filepath);
        cereal::JSONInputArchive archive(is);
        serialize(archive);
    }
}

void Editor::runOGL() {
    // retrieve the application settings from the config file
    serializeSettings("config.json");

    int sdlError = SDL_Init(SDL_INIT_VIDEO);
    m_assert(sdlError == 0, "failed to init SDL for video");

    Uint32 wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL |
        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED;

    // init scripting language
    auto chai = create_chaiscript();

    std::vector<SDL_Rect> displays;
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        displays.push_back(SDL_Rect());
        SDL_GetDisplayBounds(i, &displays.back());
    }

    // if our display setting is higher than the nr of displays we pick the default display
    display = display > displays.size() - 1 ? 0 : display;
    auto sdlWindow = SDL_CreateWindow(name.c_str(),
        displays[display].x,
        displays[display].y,
        displays[display].w,
        displays[display].h,
        wflags);

    SDL_SetWindowInputFocus(sdlWindow);

    Viewport viewport = Viewport(displays[display]);

    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // create the renderer object that does sets up the API and does all the runtime magic
    Renderer::setAPI(RenderAPI::OPENGL);
    Renderer::Init(sdlWindow);

    std::unique_ptr<Mesh> cube;
    cube.reset(new Mesh(Shape::Cube));
    cube->getVertexBuffer()->setLayout({
        {"POSITION",    ShaderType::FLOAT3},
        {"UV",          ShaderType::FLOAT2},
        {"NORMAL",      ShaderType::FLOAT3},
        {"TANGENT",     ShaderType::FLOAT3},
        {"BINORMAL",    ShaderType::FLOAT3},
    });

    auto cubeMesh = std::make_unique<ecs::MeshComponent>();


    std::unique_ptr<Mesh> unitCube;
    unitCube.reset(new Mesh());
    unitCube->setVertexBuffer(unitCubeVertices);
    unitCube->setIndexBuffer(cubeIndices);
    unitCube->getVertexBuffer()->setLayout({
        {"POSITION",    ShaderType::FLOAT3},
        {"UV",          ShaderType::FLOAT2},
        {"NORMAL",      ShaderType::FLOAT3},
        {"TANGENT",     ShaderType::FLOAT3},
        {"BINORMAL",    ShaderType::FLOAT3},
    });

    std::unique_ptr<Mesh> Quad;
    Quad.reset(new Mesh(Shape::Quad));
    Quad->getVertexBuffer()->setLayout({
        {"POSITION",    ShaderType::FLOAT3},
        {"UV",          ShaderType::FLOAT2},
        {"NORMAL",      ShaderType::FLOAT3},
        {"TANGENT",     ShaderType::FLOAT3},
        {"BINORMAL",    ShaderType::FLOAT3},
    });

    viewport.size.x = 2003, viewport.size.y = 1370;
    constexpr unsigned int SHADOW_WIDTH = 4096, SHADOW_HEIGHT = 4096;

    // get GUI i/o and set a bunch of settings
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigDockingWithShift = true;

    // set UI font that's saved in config 
    ImFont* pFont = io.Fonts->AddFontFromFileTTF(font.c_str(), 15.0f);
    if (!io.Fonts->Fonts.empty()) {
        io.FontDefault = io.Fonts->Fonts.back();
    }

    // load the UI's theme from config
    ImVec4* colors = ImGui::GetStyle().Colors;
    for (unsigned int i = 0; i < themeColors.size(); i++) {
        auto& savedColor = themeColors[i];
        ImGuiCol_Text;
        colors[i] = ImVec4(savedColor[0], savedColor[1], savedColor[2], savedColor[3]);
    }

    colors[ImGuiCol_DockingEmptyBg] = ImVec4(1, 0, 0, 1);

    ImGui::GetStyle().WindowRounding = 0.0f;
    ImGui::GetStyle().ChildRounding = 0.0f;
    ImGui::GetStyle().FrameRounding = 0.0f;
    ImGui::GetStyle().GrabRounding = 0.0f;
    ImGui::GetStyle().PopupRounding = 0.0f;
    ImGui::GetStyle().ScrollbarRounding = 0.0f;
    ImGui::GetStyle().WindowBorderSize = 0.0f;
    ImGui::GetStyle().ChildBorderSize = 0.0f;
    ImGui::GetStyle().FrameBorderSize = 0.0f;

    // timer for keeping frametime
    Timer deltaTimer;
    double deltaTime = 0;

    // setup the camera that acts as the sun's view (directional light)
    std::cout << "Creating render passes..." << std::endl;

    // all render passes
    auto lightingPass           = std::make_unique<RenderPass::DeferredLighting>(viewport);
    auto shadowMapPass          = std::make_unique<RenderPass::ShadowMap>(SHADOW_WIDTH, SHADOW_HEIGHT);
    auto tonemappingPass        = std::make_unique<RenderPass::Tonemapping>(viewport);
    auto geometryBufferPass     = std::make_unique<RenderPass::GeometryBuffer>(viewport);
    auto aabbDebugPass          = std::make_unique<RenderPass::BoundingBoxDebug>(viewport);
    auto ConeTracePass          = std::make_unique<RenderPass::ForwardLightingPass>(viewport);
    auto voxelizePass           = std::make_unique<RenderPass::Voxelization>(128);
    auto voxelDebugPass         = std::make_unique<RenderPass::VoxelizationDebug>(viewport);
    auto skyPass                = std::make_unique<RenderPass::SkyPass>(viewport);
    auto skinningPass           = std::make_unique<RenderPass::Skinning>();
    auto environmentPass        = std::make_unique<RenderPass::EnvironmentPass>();


    // boolean settings needed for a couple things
    bool doSSAO = false, doBloom = false, debugVoxels = false, doDeferred = true;
    bool mouseInViewport = false, gizmoEnabled = false, showSettingsWindow = false;

    // keep a pointer to the texture that's rendered to the window
    unsigned int activeScreenTexture = tonemappingPass->result;

    // create thread pool
    int coreCount = std::thread::hardware_concurrency();
    int threadCount = std::max(1, coreCount - 1);
    auto dispatcher = AsyncDispatcher(threadCount);

    // create empty scene
    Scene scene;
    entt::entity active = entt::null;
    //scene.openFromFile("spheres.scene");

    auto defaultMaterialEntity = scene->create();
    auto& defaultMaterialName = scene->emplace<ecs::NameComponent>(defaultMaterialEntity, "Default Material");
    auto& defaultMaterial = scene->emplace<ecs::MaterialComponent>(defaultMaterialEntity);
    defaultMaterial.uploadRenderData();

    std::cout << "Initialization done." << std::endl;

    SDL_ShowWindow(sdlWindow);
    SDL_MaximizeWindow(sdlWindow);

    gui::Guizmo gizmo;
    gui::EntityWindow ecsWindow;
    gui::AssetWindow assetBrowser;
    gui::ConsoleWindow consoleWindow;
    gui::InspectorWindow inspectorWindow;

    bool shouldVoxelize = true;

    while (running) {
        deltaTimer.start();

        scene.updateTransforms();
        
        auto animationView = scene->view<ecs::MeshAnimationComponent>();
        std::for_each(std::execution::par_unseq, animationView.begin(), animationView.end(), [&](auto entity) {
            auto& animation = animationView.get<ecs::MeshAnimationComponent>(entity);
            animation.boneTransform(static_cast<float>(deltaTime));
        });

        scene->view<ecs::MeshAnimationComponent, ecs::MeshComponent>().each([&](auto& animation, auto& mesh) {
            skinningPass->execute(mesh, animation);
        });

        handleEvents(sdlWindow, viewport.getCamera(), mouseInViewport, deltaTime);
        viewport.getCamera().update(true);

        // clear the main window
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // generate sun shadow map 
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        shadowMapPass->execute(scene);

        if (shouldVoxelize) {
            voxelizePass->execute(scene, viewport, shadowMapPass.get());
        }
        
        glViewport(0, 0, viewport.size.x, viewport.size.y);

        if (doDeferred) {
            if (!scene->view<ecs::MeshComponent>().empty()) {
                geometryBufferPass->execute(scene, viewport);
                lightingPass->execute(scene, viewport, shadowMapPass.get(), nullptr, geometryBufferPass.get(), nullptr, voxelizePass.get(), Quad.get());
                tonemappingPass->execute(lightingPass->result, Quad.get());
            }
        } else {
            if (!scene->view<ecs::MeshComponent>().empty()) {
                ConeTracePass->execute(viewport, scene, voxelizePass.get(), shadowMapPass.get());
                tonemappingPass->execute(ConeTracePass->result, Quad.get());
            }
        }

        if (active != entt::null) {
            aabbDebugPass->execute(scene, viewport, tonemappingPass->result, geometryBufferPass->GDepthBuffer,  active);
        }

        if (debugVoxels) {
            voxelDebugPass->execute(viewport, tonemappingPass->result, voxelizePass.get());
        }

        //get new frame for ImGui and ImGuizmo
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        Renderer::ImGuiNewFrame(sdlWindow);
        ImGuizmo::BeginFrame();

        if (ImGui::IsAnyItemActive()) {
            // perform input mapping
        }

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags dockWindowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        ImGuiViewport* imGuiViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(imGuiViewport->Pos);
        ImGui::SetNextWindowSize(imGuiViewport->Size);
        ImGui::SetNextWindowViewport(imGuiViewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        dockWindowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the pass-thru hole, 
        // so we ask Begin() to not render a background.
        ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) dockWindowFlags |= ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace", (bool*)true, dockWindowFlags);
        ImGui::PopStyleVar();
        ImGui::PopStyleVar(2);

        // DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        // draw the top user bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if(ImGui::MenuItem("New Scene")) {
                    scene->clear();
                    active = entt::null;
                }

                if (ImGui::MenuItem("Open scene..")) {
                    std::string filepath = OS::openFileDialog("Scene Files (*.scene)\0*.scene\0");
                    if (!filepath.empty()) {
                        SDL_SetWindowTitle(sdlWindow, std::string(filepath + " - Raekor Renderer").c_str());
                        scene.openFromFile(filepath);
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
                        AssimpImporter::loadFile(scene, filepath);
                    }
                }

                if (ImGui::MenuItem("Save Screenshot..")) {
                    std::string savePath = OS::saveFileDialog("Uncompressed PNG (*.png)\0", "png");

                    if (!savePath.empty()) {
                        const auto bufferSize = 4 * viewport.size.x * viewport.size.y;
                        auto pixels = std::vector<unsigned char>(bufferSize);
                        glGetTextureImage(tonemappingPass->result, 0, GL_RGBA, GL_UNSIGNED_BYTE, bufferSize* sizeof(unsigned char), pixels.data());
                        stbi_flip_vertically_on_write(true);
                        stbi_write_png(savePath.c_str(), viewport.size.x, viewport.size.y, 4, pixels.data(), viewport.size.x * 4);
                    }

                }

                if (ImGui::MenuItem("Exit", "Escape")) {
                    running = false;
                }

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Delete", "DEL")) {
                    // on press we remove the scene object
                    if (active != entt::null) {
                        if (scene->has<ecs::NodeComponent>(active)) {
                            scene.destroyObject(active);
                        } else {
                            scene->destroy(active);
                        }
                        active = entt::null;
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Add")) {
                if (ImGui::MenuItem("Empty", "CTRL+E")) {
                    auto entity = scene.createObject("Empty");

                    if (active != entt::null) {
                        auto& node = scene->get<ecs::NodeComponent>(entity);
                        node.parent = active;
                        node.hasChildren = false;
                        scene->get<ecs::NodeComponent>(node.parent).hasChildren = true;
                    }

                    active = entity;
                }
                ImGui::Separator();

                if (ImGui::MenuItem("Sphere")) {
                    auto entity = scene.createObject("Sphere");
                    auto& mesh = scene->emplace<ecs::MeshComponent>(entity);

                    if (active != entt::null) {
                        auto& node = scene->get<ecs::NodeComponent>(entity);
                        node.parent = active;
                        node.hasChildren = false;
                        scene->get<ecs::NodeComponent>(node.parent).hasChildren = true;
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

                    for (int i = 0; i <= stackCount; ++i)
                    {
                        stackAngle = PI / 2 - i * stackStep;        // starting from pi/2 to -pi/2
                        xy = radius * cosf(stackAngle);             // r * cos(u)
                        z = radius * sinf(stackAngle);              // r * sin(u)

                        // add (sectorCount+1) vertices per stack
                        // the first and last vertices have same position and normal, but different tex coords
                        for (int j = 0; j <= sectorCount; ++j)
                        {
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
                    for (int i = 0; i < stackCount; ++i)
                    {
                        k1 = i * (sectorCount + 1);     // beginning of current stack
                        k2 = k1 + sectorCount + 1;      // beginning of next stack

                        for (int j = 0; j < sectorCount; ++j, ++k1, ++k2)
                        {
                            // 2 triangles per sector excluding first and last stacks
                            // k1 => k2 => k1+1
                            if (i != 0)
                            {
                                mesh.indices.push_back(k1);
                                mesh.indices.push_back(k2);
                                mesh.indices.push_back(k1 + 1);
                            }

                            // k1+1 => k2 => k2+1
                            if (i != (stackCount - 1))
                            {
                                mesh.indices.push_back(k1 + 1);
                                mesh.indices.push_back(k2);
                                mesh.indices.push_back(k2 + 1);
                            }
                        }
                    }

                    mesh.uploadVertices();
                    mesh.uploadIndices();
                    mesh.generateAABB();
                    mesh.material = defaultMaterialEntity;
                }

                if (ImGui::BeginMenu("Light")) {

                    if (ImGui::MenuItem("Directional Light")) {
                        auto entity = scene.createObject("Directional Light");
                        auto& transform = scene->get<ecs::TransformComponent>(entity);
                        transform.rotation.x = static_cast<float>(M_PI / 12);
                        transform.recalculateMatrix();
                        scene->emplace<ecs::DirectionalLightComponent>(entity);
                        active = entity;
                    }

                    if (ImGui::MenuItem("Point Light")) {
                        auto entity = scene.createObject("Point Light");
                        scene->emplace<ecs::PointLightComponent>(entity);
                        active = entity;
                    }

                    ImGui::EndMenu();
                }


                ImGui::EndMenu();
            }


            if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete), true)) {
                if (active != entt::null) {
                    if (scene->has<ecs::NodeComponent>(active)) {
                        scene.destroyObject(active);
                    }
                    else {
                        scene->destroy(active);
                    }
                    active = entt::null;
                }
            }

            if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_C), true)) {
                if (SDL_GetModState() & KMOD_LCTRL) {
                    auto newEntity = scene->create();

                    scene->visit(active, [&](const auto& component) {
                        auto clone_function = ecs::cloner::getSingleton()->getFunction(component);
                        if (clone_function) {
                            clone_function(scene, active, newEntity);
                        }
                    });
                }
            }

            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About", "")) {}
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        assetBrowser.drawWindow(scene, active);

        // chai console panel
        consoleWindow.Draw(chai.get());

        //Inspector panel
        inspectorWindow.draw(scene, active);

        // scene / ecs panel
        ecsWindow.draw(scene, active);

        // post processing panel
        ImGui::Begin("Post Processing");
        static bool doTonemapping = true;
        if (ImGui::Checkbox("Tonemap", &doTonemapping)) {
            if (doTonemapping) {
                activeScreenTexture = tonemappingPass->result;
            } else {
                activeScreenTexture = lightingPass->result;
            }
        }
        ImGui::Separator();

        if (ImGui::SliderFloat("Exposure", &tonemappingPass->settings.exposure, 0.0f, 1.0f)) {}
        if (ImGui::SliderFloat("Gamma", &tonemappingPass->settings.gamma, 1.0f, 3.2f)) {}
        ImGui::NewLine();

        if (ImGui::Checkbox("Bloom", &doBloom)) {}
        ImGui::Separator();

        if (ImGui::DragFloat3("Threshold", glm::value_ptr(lightingPass->settings.bloomThreshold), 0.001f, 0.0f, 10.0f)) {}
        ImGui::NewLine();

        ImGui::End();

        // scene panel
        ImGui::Begin("Random");
        ImGui::SetItemDefaultFocus();

        // toggle button for openGl vsync
        static bool doVsync = true;
        if (ImGui::RadioButton("Vsync", doVsync)) {
            doVsync = !doVsync;
        }

        ImGui::NewLine(); ImGui::Separator(); 
        ImGui::Text("Voxel Cone Tracing");

        if (ImGui::RadioButton("Debug", debugVoxels)) {
            debugVoxels = !debugVoxels;
        }

        if (ImGui::RadioButton("Voxelize", shouldVoxelize)) {
            shouldVoxelize = !shouldVoxelize;
        }

        if (ImGui::RadioButton("Deferred", doDeferred)) {
            doDeferred = !doDeferred;
        }

        ImGui::DragFloat("World size", &voxelizePass->worldSize, 0.05f, 1.0f, FLT_MAX, "%.2f");

        ImGui::Separator();

        if (ImGui::TreeNode("Screen Texture")) {
            if (ImGui::Selectable(nameof(tonemappingPass->result), activeScreenTexture == tonemappingPass->result))
                activeScreenTexture = tonemappingPass->result;
            if (ImGui::Selectable(nameof(skyPass->result), activeScreenTexture == skyPass->result))
                activeScreenTexture = skyPass->result;
            if (ImGui::Selectable(nameof(geometryBufferPass->albedoTexture), activeScreenTexture == geometryBufferPass->albedoTexture))
                activeScreenTexture = geometryBufferPass->albedoTexture;
            if (ImGui::Selectable(nameof(geometryBufferPass->normalTexture), activeScreenTexture == geometryBufferPass->normalTexture))
                activeScreenTexture = geometryBufferPass->normalTexture;
            if (ImGui::Selectable(nameof(geometryBufferPass->materialTexture), activeScreenTexture == geometryBufferPass->materialTexture))
                activeScreenTexture = geometryBufferPass->materialTexture;
            if (ImGui::Selectable(nameof(geometryBufferPass->positionTexture), activeScreenTexture == geometryBufferPass->positionTexture))
                activeScreenTexture = geometryBufferPass->positionTexture;
            ImGui::TreePop();
        }

        ImGui::NewLine();

        ImGui::Text("Shadow Mapping");
        ImGui::Separator();
        
        if (ImGui::DragFloat2("Planes", glm::value_ptr(shadowMapPass->settings.planes), 0.1f)) {}
        if (ImGui::DragFloat("Size", &shadowMapPass->settings.size)) {}
        if (ImGui::DragFloat("Min bias", &lightingPass->settings.minBias, 0.0001f, 0.0f, FLT_MAX, "%.4f")) {}
        if (ImGui::DragFloat("Max bias", &lightingPass->settings.maxBias, 0.0001f, 0.0f, FLT_MAX, "%.4f")) {}
        
        ImGui::NewLine();
        ImGui::Separator();
        ImGui::NewLine();
        ImGui::Text("Sky Settings");
        ImGui::DragFloat("time", &skyPass->settings.time, 0.01f, 0.0f, 1000.0f);
        ImGui::DragFloat("cumulus", &skyPass->settings.cumulus, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("cirrus", &skyPass->settings.cirrus, 0.01f, 0.0f, 1.0f);
        ImGui::NewLine();

        ImGui::End();

        ImGui::Begin("Camera Properties");
        if (ImGui::DragFloat("FoV", &viewport.getFov(), 1.0f, 35.0f, 120.0f)) {
            viewport.setFov(viewport.getFov());
        }
        if (ImGui::DragFloat("Move Speed", &viewport.getCamera().moveSpeed, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}
        if (ImGui::DragFloat("Move Constant", &viewport.getCamera().moveConstant, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}
        if (ImGui::DragFloat("Look Speed", &viewport.getCamera().lookSpeed, 0.1f, 0.0001f, FLT_MAX, "%.4f")) {}
        if (ImGui::DragFloat("Look Constant", &viewport.getCamera().lookConstant, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}
        if (ImGui::DragFloat("Zoom Speed", &viewport.getCamera().zoomSpeed, 0.001f, 0.0001f, FLT_MAX, "%.4f")) {}
        if (ImGui::DragFloat("Zoom Constant", &viewport.getCamera().zoomConstant, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}

        ImGui::End();

        gizmo.drawWindow();
        
        // renderer viewport
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Renderer", NULL, ImGuiWindowFlags_AlwaysAutoResize);

        // figure out if we need to resize the viewport
        static bool resizing = false;
        auto size = ImGui::GetContentRegionAvail();
        if (viewport.size.x != size.x || viewport.size.y != size.y) {
            resizing = true;
            viewport.size.x = static_cast<uint32_t>(size.x), viewport.size.y = static_cast<uint32_t>(size.y);
        }
        auto pos = ImGui::GetWindowPos();

        // determine if the mouse is hovering the viewport 
        if (ImGui::IsWindowHovered()) {
            mouseInViewport = true;
        } else {
            mouseInViewport = false;
        }

        if (io.MouseClicked[0] && mouseInViewport && !(active != entt::null && ImGuizmo::IsOver(gizmo.getOperation()))) {
            // get mouse position in window
            glm::ivec2 mousePosition;
            SDL_GetMouseState(&mousePosition.x, &mousePosition.y);

            // get mouse position relative to viewport
            glm::ivec2 rendererMousePosition = { (mousePosition.x - pos.x), (mousePosition.y - pos.y) };

            // flip mouse coords for opengl
            rendererMousePosition.y = viewport.size.y - rendererMousePosition.y;

            entt::entity picked = entt::null;
            if (doDeferred) {
                picked = geometryBufferPass->pick(rendererMousePosition.x, rendererMousePosition.y);
            } else {
                picked = ConeTracePass->pick(rendererMousePosition.x, rendererMousePosition.y);
            }

            if (scene->valid(picked)) {
                active = active == picked ? entt::null : picked;
            } else {
                active = entt::null;
            }
        }

        // render the active screen texture to the view port as an imgui image
        ImGui::Image((void*)((intptr_t)activeScreenTexture), ImVec2((float)viewport.size.x, (float)viewport.size.y), { 0,1 }, { 1,0 });

        // draw the imguizmo at the center of the active entity
        if (active != entt::null) {
            gizmo.drawGuizmo(scene, viewport, active);
        }

        ImGui::End();
        ImGui::PopStyleVar();

        // application/render metrics
        ImGui::SetNextWindowBgAlpha(0.35f);
        ImGui::SetNextWindowPos(ImVec2(pos.x + size.x - size.x / 7.5f - 5.0f, pos.y + 5.0f));
        ImGui::SetNextWindowSize(ImVec2(size.x / 7.5f, size.y / 9.0f));
        auto metricWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration;
        ImGui::Begin("GPU Metrics", (bool*)0, metricWindowFlags);
        ImGui::Text("Vendor: %s", glGetString(GL_VENDOR));
        ImGui::Text("Product: %s", glGetString(GL_RENDERER));
        ImGui::Text("Resolution: %i x %i", viewport.size.x, viewport.size.y);
        ImGui::Text("Frame %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        int culledCount = doDeferred ? geometryBufferPass->culled : ConeTracePass->culled;
        ImGui::Text("Culling: %i of %i meshes", culledCount, scene->view<ecs::MeshComponent>().size());
        ImGui::Text("Graphics API: OpenGL %s", glGetString(GL_VERSION));
        ImGui::End();


        ImGui::End();
        Renderer::ImGuiRender();
        Renderer::SwapBuffers(doVsync);

        if (resizing) {
            // adjust the camera and gizmo
            viewport.getCamera().getProjection() = glm::perspectiveRH(glm::radians(viewport.getFov()), (float)viewport.size.x / (float)viewport.size.y, 0.1f, 10000.0f);
            ImGuizmo::SetRect(pos.x, pos.y, size.x, size.y);

            // resizing framebuffers
            lightingPass->deleteResources();
            lightingPass->createResources(viewport);

            ConeTracePass->deleteResources();
            ConeTracePass->createResources(viewport);

            aabbDebugPass->deleteResources();
            aabbDebugPass->createResources(viewport);

            voxelDebugPass->deleteResources();
            voxelDebugPass->createResources(viewport);

            tonemappingPass->deleteResources();
            tonemappingPass->createResources(viewport);

            geometryBufferPass->deleteResources();
            geometryBufferPass->createResources(viewport);

            
            resizing = false;
        }

        deltaTimer.stop();
        deltaTime = deltaTimer.elapsedMs();

    } // while true loop

    display = SDL_GetWindowDisplayIndex(sdlWindow);
    //clean up SDL
    SDL_DestroyWindow(sdlWindow);
    SDL_Quit();

} // application::run

} // namespace Raekor  