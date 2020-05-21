#include "pch.h"
#include "app.h"
#include "ecs.h"
#include "util.h"
#include "scene.h"
#include "entry.h"
#include "camera.h"
#include "shader.h"
#include "framebuffer.h"
#include "OS.h"
#include "renderer.h"
#include "buffer.h"
#include "timer.h"
#include "renderpass.h"
#include "gui.h"

namespace Raekor {

void Application::serializeSettings(const std::string& filepath, bool write) {
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

void Application::run() {
    // retrieve the application settings from the config file
    serializeSettings("config.json");

    int sdlError = SDL_Init(SDL_INIT_VIDEO);
    m_assert(sdlError == 0, "failed to init SDL for video");

    Uint32 wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL |
        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED;

    // init scripting language
    chaiscript::ChaiScript chai = chaiscript::ChaiScript({}, { "scripts\\" });

    // add glm::vec2
    chai.add(chaiscript::user_type<glm::vec<2, float, glm::packed_highp>>(), "Vector2");
    chai.add(chaiscript::constructor<glm::vec<2, float, glm::packed_highp>(float x, float y)>(), "Vector2");
    chai.add(chaiscript::fun(&glm::vec<2, float, glm::packed_highp>::x), "x");
    chai.add(chaiscript::fun(&glm::vec<2, float, glm::packed_highp>::y), "y");

    // add glm::vec3
    chai.add(chaiscript::user_type<glm::vec<2, float, glm::packed_highp>>(), "Vector3");
    chai.add(chaiscript::constructor<glm::vec<3, float, glm::packed_highp>(float x, float y, float z)>(), "Vector3");

    chai.add(chaiscript::fun(static_cast<glm::vec<3, float, glm::packed_highp> & (glm::vec<3, float, glm::packed_highp>::*)(const glm::vec<3, float, glm::packed_highp>&)>(&glm::vec<3, float, glm::packed_highp>::operator=)), "=");
    chai.add(chaiscript::fun(static_cast<glm::vec<2, float, glm::packed_highp> & (glm::vec<2, float, glm::packed_highp>::*)(const glm::vec<2, float, glm::packed_highp>&)>(&glm::vec<2, float, glm::packed_highp>::operator=)), "=");


    chai.add(chaiscript::fun(static_cast<glm::vec<3, float, glm::packed_highp> & (glm::vec<3, float, glm::packed_highp>::*)(const glm::vec<3, float, glm::packed_highp>&)>(&glm::vec<3, float, glm::packed_highp>::operator+=)), "+=");

    chai.add(chaiscript::bootstrap::standard_library::vector_type<std::vector<Vertex>>("VertexList"));
    chai.add(chaiscript::bootstrap::standard_library::vector_type<std::vector<Triangle>>("FaceList"));

    chai.add(chaiscript::fun(&glm::vec<3, float, glm::packed_highp>::x), "x");
    chai.add(chaiscript::fun(&glm::vec<3, float, glm::packed_highp>::y), "y");
    chai.add(chaiscript::fun(&glm::vec<3, float, glm::packed_highp>::z), "z");

    // add Vertex
    chai.add(chaiscript::user_type<Vertex>(), "Vertex");
    chai.add(chaiscript::constructor<Vertex()>(), "Vertex");
    chai.add(chaiscript::fun(&Vertex::pos), "pos");
    chai.add(chaiscript::fun(&Vertex::uv), "uv");
    chai.add(chaiscript::fun(&Vertex::normal), "normal");

     //add Face
    chai.add(chaiscript::user_type<Triangle>(), "Triangle");
    chai.add(chaiscript::constructor<Triangle()>(), "Triangle");
    chai.add(chaiscript::constructor<Triangle(uint32_t _f1, uint32_t _f2, uint32_t _f3)>(), "Triangle");
    chai.add(chaiscript::fun(&Triangle::p1), "p1");
    chai.add(chaiscript::fun(&Triangle::p2), "p2");
    chai.add(chaiscript::fun(&Triangle::p3), "p3");

    // add cross product for vec3's
    chai.add(chaiscript::fun(static_cast<glm::vec<3, float, glm::packed_highp>(*)(const glm::vec<3, float, glm::packed_highp>&, const glm::vec<3, float, glm::packed_highp>&)>(&glm::cross<float, glm::packed_highp>)), "cross");

    // add normalize for vector 3
    chai.add(chaiscript::fun(static_cast<glm::vec<3, float, glm::packed_highp>(*)(const glm::vec<3, float, glm::packed_highp>&)>(&glm::normalize<3, float, glm::packed_highp>)), "normalize");

    // add minus operator for vector 3's
    chai.add(chaiscript::fun(static_cast<glm::vec<3, float, glm::packed_highp>(*)(const glm::vec<3, float, glm::packed_highp>&, const glm::vec<3, float, glm::packed_highp>&)>(&glm::operator-<float, glm::packed_highp>)), "-");

    glm::vec<3, float, glm::packed_highp>(*fff)(const glm::vec<3, float, glm::packed_highp>&, const glm::vec<3, float, glm::packed_highp>&) = glm::operator-<float, glm::packed_highp>;

    // add glm::perlin for vec2's
    chai.add(chaiscript::fun(static_cast<float(*)(glm::vec<2, float, glm::packed_highp> const&)>(&glm::perlin<float, glm::packed_highp>)), "perlin");
    chai.add(chaiscript::fun(static_cast<float(*)(glm::vec<2, float, glm::packed_highp> const&)>(&glm::simplex<float, glm::packed_highp>)), "simplex");

    // add scene methods
    Scene newScene;

    // add scene type
    chai.add(chaiscript::var(std::ref(newScene)), "scene");

    chai.add(chaiscript::fun(&Scene::addMesh), "addMesh");
    chai.add(chaiscript::fun(&Scene::createPointLight), "createPointLight");
    chai.add(chaiscript::fun(&Scene::createObject), "createObject");

    chai.add(chaiscript::fun(&Scene::meshes), "meshes");

    chai.add(chaiscript::fun(&ECS::ComponentManager<ECS::MeshComponent>::create), "create");


    chai.add(chaiscript::fun(&ECS::MeshComponent::vertices), "vertices");
    chai.add(chaiscript::fun(&ECS::MeshComponent::indices), "indices");
    chai.add(chaiscript::fun(&ECS::MeshComponent::uploadVertices), "uploadVertices");
    chai.add(chaiscript::fun(&ECS::MeshComponent::uploadIndices), "uploadIndices");
    chai.add(chaiscript::fun(&ECS::MeshComponent::generateAABB), "generateAABB");

    std::vector<SDL_Rect> displays;
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        displays.push_back(SDL_Rect());
        SDL_GetDisplayBounds(i, &displays.back());
    }

    // if our display setting is higher than the nr of displays we pick the default display
    display = display > displays.size() - 1 ? 0 : display;
    auto directxwindow = SDL_CreateWindow(name.c_str(),
        displays[display].x,
        displays[display].y,
        displays[display].w,
        displays[display].h,
        wflags);

    SDL_SetWindowInputFocus(directxwindow);

    Viewport viewport = Viewport(displays[display]);

    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // create the renderer object that does sets up the API and does all the runtime magic
    Renderer::setAPI(RenderAPI::OPENGL);
    Renderer::Init(directxwindow);

    try {
        //chai.eval_file("perlin.chai");
    }
    catch (const chaiscript::exception::eval_error& ee) {
        std::cout << ee.what();
        if (ee.call_stack.size() > 0) {
            std::cout << "during evaluation at (" << ee.call_stack[0].start().line << ", " << ee.call_stack[0].start().column << ")";
        }
        std::cout << '\n';
    }

    Ffilter meshFileFormats;
    meshFileFormats.name = "Supported Mesh Files";
    meshFileFormats.extensions = "*.obj;*.fbx;*.gltf;*.glb";

    Ffilter textureFileFormats;
    textureFileFormats.name = "Supported Image Files";
    textureFileFormats.extensions = "*.png;*.jpg;*.jpeg;*.tga";

    std::unique_ptr<Mesh> cube;
    cube.reset(new Mesh(Shape::Cube));
    cube->getVertexBuffer()->setLayout({
        {"POSITION",    ShaderType::FLOAT3},
        {"UV",          ShaderType::FLOAT2},
        {"NORMAL",      ShaderType::FLOAT3},
        {"TANGENT",     ShaderType::FLOAT3},
        {"BINORMAL",    ShaderType::FLOAT3}
    });

    std::unique_ptr<Mesh> Quad;
    Quad.reset(new Mesh(Shape::Quad));
    Quad->getVertexBuffer()->setLayout({
        {"POSITION",    ShaderType::FLOAT3},
        {"UV",          ShaderType::FLOAT2},
        {"NORMAL",      ShaderType::FLOAT3},
        {"TANGENT",     ShaderType::FLOAT3},
        {"BINORMAL",    ShaderType::FLOAT3}
    });

    viewport.size.x = 2003, viewport.size.y = 1370;
    constexpr unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;

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
        colors[i] = ImVec4(savedColor[0], savedColor[1], savedColor[2], savedColor[3]);
    }

    ImGui::GetStyle().WindowRounding = 0.0f;
    ImGui::GetStyle().ChildRounding = 0.0f;
    ImGui::GetStyle().FrameRounding = 0.0f;
    ImGui::GetStyle().GrabRounding = 0.0f;
    ImGui::GetStyle().PopupRounding = 0.0f;
    ImGui::GetStyle().ScrollbarRounding = 0.0f;

    // timer for keeping frametime
    Timer deltaTimer;
    double deltaTime = 0;

    // setup the camera that acts as the sun's view (directional light)
    std::cout << "Creating render passes..." << std::endl;

    // all render passes
    auto bloomPass              = std::make_unique<RenderPass::Bloom>(viewport);
    auto lightingPass           = std::make_unique<RenderPass::DeferredLighting>(viewport);
    auto shadowMapPass          = std::make_unique<RenderPass::ShadowMap>(SHADOW_WIDTH, SHADOW_HEIGHT);
    auto tonemappingPass        = std::make_unique<RenderPass::Tonemapping>(viewport);
    auto omniShadowMapPass      = std::make_unique<RenderPass::OmniShadowMap>(SHADOW_WIDTH, SHADOW_HEIGHT);
    auto geometryBufferPass     = std::make_unique<RenderPass::GeometryBuffer>(viewport);
    auto ambientOcclusionPass   = std::make_unique<RenderPass::ScreenSpaceAmbientOcclusion>(viewport);

    auto voxelizationPass       = std::make_unique<RenderPass::Voxelization>(512, 512, 512);
    auto voxelDebugPass         = std::make_unique<RenderPass::VoxelizationDebug>(viewport);

    auto aabbDebugPass          = std::make_unique<RenderPass::BoundingBoxDebug>(viewport);


    // boolean settings needed for a couple things
    bool doSSAO = true, doBloom = false;
    bool mouseInViewport = false, gizmoEnabled = false, showSettingsWindow = false;

    // keep a pointer to the texture that's rendered to the window
    glTexture2D* activeScreenTexture = &tonemappingPass->result;

    AssimpImporter importer;
    static ECS::Entity active = NULL;
    for (const std::string& path : project) {
        std::cout << "Loading " << parseFilepath(path, PATH_OPTIONS::FILENAME) << "...\n";
        importer.loadFromDisk(newScene, path);
    }

    auto pointLight = newScene.createPointLight("Point Light");
    auto pointLightTransform = newScene.transforms.getComponent(pointLight);
    pointLightTransform->position.z = 3.0f;
    pointLightTransform->position.y = 1.0f;
    pointLightTransform->recalculateMatrix();


    auto dirLightEntity = newScene.createDirectionalLight("Directional Light");
    auto transform = newScene.transforms.getComponent(dirLightEntity);
    transform->position.y = 15.0f;
    transform->recalculateMatrix();

    std::cout << "Initialization done." << std::endl;

    SDL_ShowWindow(directxwindow);
    SDL_MaximizeWindow(directxwindow);

    GUI::Guizmo gizmo;
    GUI::EntityWindow ecsWindow;
    GUI::ConsoleWindow consoleWindow;
    GUI::InspectorWindow inspectorWindow;

    ImVec2 pos;

    while (running) {
        deltaTimer.start();

        // if we're debugging the shadow map we directly control the sun camera
        if (activeScreenTexture != &shadowMapPass->result)
            handleEvents(directxwindow, viewport.getCamera(), mouseInViewport, deltaTime);
        else
            handleEvents(directxwindow, shadowMapPass->sunCamera, mouseInViewport, deltaTime);
        
        shadowMapPass->sunCamera.update(true);
        viewport.getCamera().update(true);

        // clear the main window
        Renderer::Clear({ 0.22f, 0.32f, 0.42f, 1.0f });
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

        // generate sun shadow map 
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        shadowMapPass->execute(newScene);

        // generate point light shadow map 
        omniShadowMapPass->execute(newScene, pointLightTransform->position);

        // generate a geometry buffer
        glViewport(0, 0, viewport.size.x, viewport.size.y);
        geometryBufferPass->execute(newScene, viewport);

        if (doSSAO) {
            ambientOcclusionPass->execute(viewport, geometryBufferPass.get(), Quad.get());
        }

        // perform deferred lighting pass
        lightingPass->execute(
            newScene,
            viewport,
            shadowMapPass.get(), 
            omniShadowMapPass.get(),
            geometryBufferPass.get(),
            ambientOcclusionPass.get(),
            Quad.get()
        );

        // perform Bloom
        if (doBloom) {
            bloomPass->execute(lightingPass->result, lightingPass->bloomHighlights, Quad.get());
            tonemappingPass->execute(bloomPass->result, Quad.get());
        }
        else {
            tonemappingPass->execute(lightingPass->result, Quad.get());
        }

        if (active) {
            aabbDebugPass->execute(newScene, viewport, tonemappingPass->result, active);
        }

        // generate texture that visualizes a 3D voxel texture 
        voxelDebugPass->execute(viewport, voxelizationPass->result, cube.get(), Quad.get());
        
        //get new frame for ImGui and ImGuizmo
        Renderer::ImGuiNewFrame(directxwindow);
        ImGuizmo::BeginFrame();

        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

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
        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) dockWindowFlags |= ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        static bool p_open = true;
        ImGui::Begin("DockSpace", &p_open, dockWindowFlags);
        ImGui::PopStyleVar();
        ImGui::PopStyleVar(2);

        // DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        // move the light by a fixed amount and let it bounce between -125 and 125 units/pixels on the x axis
        static double lightMoveSpeed = 0.003;
        static double bounds = 7.5f;
        static bool moveLight = false;
        double lightMoveAmount = lightMoveSpeed * deltaTime;

        // draw the top user bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Load..")) {
                    std::string path = OS::openFileDialog({ meshFileFormats });
                    if (!path.empty()) {
                        importer.loadFromDisk(newScene, path);
                    }
                }
                if (ImGui::MenuItem("Save", "CTRL + S")) {
                    display = SDL_GetWindowDisplayIndex(directxwindow);
                    serializeSettings("config.json", true);
                }

                if (ImGui::MenuItem("Save as..", "CTRL + S")) {
                    serializeSettings("config.json", true);
                }

                if (ImGui::MenuItem("Settings", "")) {
                    showSettingsWindow = true;
                }

                if (ImGui::MenuItem("Exit", "Escape")) {
                    running = false;
                }

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Delete", "DEL")) {
                    // on press we remove the scene object
                    newScene.remove(active);
                    active = NULL;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Add")) {
                if (ImGui::MenuItem("Empty", "CTRL+E")) {
                    newScene.createObject("Empty");
                }
                ImGui::Separator();

                if (ImGui::BeginMenu("Light")) {

                    if (ImGui::MenuItem("Directional Light")) {
                        newScene.createDirectionalLight("Directional Light");
                    }

                    if (ImGui::MenuItem("Point Light")) {
                        newScene.createPointLight("Point Light");
                    }

                    ImGui::EndMenu();
                }


                ImGui::EndMenu();
            }


            if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete), true)) {
                // on press we remove the scene object
                newScene.remove(active);
                active = NULL;
            }

            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About", "")) {}
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // chai console panel
        consoleWindow.Draw(chai);

        //Inspector panel
        inspectorWindow.draw(newScene, active);

        // scene / ecs panel
        ecsWindow.draw(newScene, active);

        // post processing panel
        ImGui::Begin("Post Processing");
        static bool doTonemapping = true;
        if (ImGui::Checkbox("Tonemap", &doTonemapping)) {
            if (doTonemapping) {
                activeScreenTexture = &tonemappingPass->result;
            } else {
                activeScreenTexture = &lightingPass->result;
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

        if (ImGui::Checkbox("SSAO", &doSSAO)) {
            // if we don't want to apply SSAO we need to clear the old result
            if (!doSSAO) ambientOcclusionPass->result.clear({ 1.0f, 1.0f, 1.0f, 1.0f });
        }
        ImGui::Separator();
        if (ImGui::DragFloat(   "Samples",  &ambientOcclusionPass->settings.samples, 8.0f, 8.0f, 64.0f)) {}
        if (ImGui::SliderFloat( "Power",    &ambientOcclusionPass->settings.power, 0.0f, 15.0f)) {}
        if (ImGui::SliderFloat( "Bias",     &ambientOcclusionPass->settings.bias, 0.0f, 1.0f)) {}

        ImGui::End();

        // scene panel
        ImGui::Begin("Random");
        ImGui::SetItemDefaultFocus();

        // toggle button for openGl vsync
        static bool doVsync = false;
        if (ImGui::RadioButton("USE VSYNC", doVsync)) {
            doVsync = !doVsync;
        }

        if (ImGui::RadioButton("Animate Light", moveLight)) {
            moveLight = !moveLight;
        }

        if (ImGui::TreeNode("Screen Texture")) {
            if (ImGui::Selectable(nameof(tonemappingPass->result), activeScreenTexture->ImGuiID() == tonemappingPass->result.ImGuiID()))
                activeScreenTexture = &tonemappingPass->result;
            if (ImGui::Selectable(nameof(geometryBufferPass->albedoTexture), activeScreenTexture->ImGuiID() == geometryBufferPass->albedoTexture.ImGuiID()))
                activeScreenTexture = &geometryBufferPass->albedoTexture;
            if (ImGui::Selectable(nameof(geometryBufferPass->normalTexture), activeScreenTexture->ImGuiID() == geometryBufferPass->normalTexture.ImGuiID()))
                activeScreenTexture = &geometryBufferPass->normalTexture;
            if (ImGui::Selectable(nameof(geometryBufferPass->positionTexture), activeScreenTexture->ImGuiID() == geometryBufferPass->positionTexture.ImGuiID()))
                activeScreenTexture = &geometryBufferPass->positionTexture;
            if (ImGui::Selectable(nameof(shadowMapPass->result), activeScreenTexture->ImGuiID() == shadowMapPass->result.ImGuiID()))
                activeScreenTexture = &shadowMapPass->result;
            if (ImGui::Selectable(nameof(bloomPass->result), activeScreenTexture->ImGuiID() == bloomPass->result.ImGuiID()))
                activeScreenTexture = &bloomPass->result;
            if (ImGui::Selectable(nameof(ambientOcclusionPass->result), activeScreenTexture->ImGuiID() == ambientOcclusionPass->result.ImGuiID()))
                activeScreenTexture = &ambientOcclusionPass->result;
            if (ImGui::Selectable(nameof(ambientOcclusionPass->preblurResult), activeScreenTexture->ImGuiID() == ambientOcclusionPass->preblurResult.ImGuiID()))
                activeScreenTexture = &ambientOcclusionPass->preblurResult;
            if (ImGui::Selectable(nameof(voxelDebugPass->result), activeScreenTexture->ImGuiID() == voxelDebugPass->result.ImGuiID()))
                activeScreenTexture = &voxelDebugPass->result;
            if (ImGui::Selectable(nameof(voxelDebugPass->cubeFront), activeScreenTexture->ImGuiID() == voxelDebugPass->cubeFront.ImGuiID()))
                activeScreenTexture = &voxelDebugPass->cubeFront;
            if (ImGui::Selectable(nameof(voxelDebugPass->cubeBack), activeScreenTexture->ImGuiID() == voxelDebugPass->cubeBack.ImGuiID()))
                activeScreenTexture = &voxelDebugPass->cubeBack;
            if (ImGui::Selectable(nameof(aabbDebugPass->result), activeScreenTexture->ImGuiID() == aabbDebugPass->result.ImGuiID()))
                activeScreenTexture = &aabbDebugPass->result;

            ImGui::TreePop();
        }

        ImGui::NewLine();

        ImGui::Text("Directional Light");
        ImGui::Separator();
        if (ImGui::DragFloat2("Angle", glm::value_ptr(shadowMapPass->sunCamera.getAngle()), 0.01f)) {}
        
        if (ImGui::DragFloat2("Planes", glm::value_ptr(shadowMapPass->settings.planes), 0.1f)) {
            shadowMapPass->sunCamera.getProjection() = glm::orthoRH_ZO(-shadowMapPass->settings.size, shadowMapPass->settings.size, -shadowMapPass->settings.size, shadowMapPass->settings.size,
                shadowMapPass->settings.planes.x, shadowMapPass->settings.planes.y
            );
        }
        if (ImGui::DragFloat("Size", &shadowMapPass->settings.size)) {
            shadowMapPass->sunCamera.getProjection() = glm::orthoRH_ZO(-shadowMapPass->settings.size, shadowMapPass->settings.size, -shadowMapPass->settings.size, shadowMapPass->settings.size,
                shadowMapPass->settings.planes.x, shadowMapPass->settings.planes.y
            );
        }

        if (ImGui::DragFloat("Min bias", &lightingPass->settings.minBias, 0.0001f, 0.0f, FLT_MAX, "%.4f")) {}
        if (ImGui::DragFloat("Max bias", &lightingPass->settings.maxBias, 0.0001f, 0.0f, FLT_MAX, "%.4f")) {}
        if (ImGui::ColorEdit3("Color", glm::value_ptr(lightingPass->settings.sunColor))) {}
        
        ImGui::NewLine();
        ImGui::Text("Point Light");
        ImGui::Separator();
        if (ImGui::DragFloat("far plane", &omniShadowMapPass->settings.farPlane)) {}
        
        ImGui::NewLine();
        ImGui::Text("Normal Maps");
        ImGui::Separator();

        static bool doNormalMapping = false;
        if (ImGui::Checkbox("Enable ##normalmapping", &doNormalMapping)) {
            if (!doNormalMapping) {
            
            }
            else {
            
            }
        }

        ImGui::End();

        // application/render metrics
        auto metricWindowFlags = ImGuiWindowFlags_NoTitleBar;
        ImGui::Begin("GPU Metrics", (bool*)0, metricWindowFlags);
        ImGui::Text("Vendor: %s", glGetString(GL_VENDOR));
        ImGui::Text("Product: %s", glGetString(GL_RENDERER));
        ImGui::Text("Resolution: %i x %i", viewport.size.x, viewport.size.y);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::Text("Culling: %i of %i meshes (Gbuffer only)", geometryBufferPass->culled, newScene.meshes.getCount());
        ImGui::Text("Graphics API: OpenGL %s", glGetString(GL_VERSION));
        ImGui::End();

        ImGui::Begin("Camera Properties");
        static float fov = 45.0f;
        if (ImGui::DragFloat("FoV", &fov, 1.0f, 35.0f, 120.0f)) {
            viewport.getCamera().getProjection() = glm::perspectiveRH(glm::radians(fov), (float)viewport.size.x / (float)viewport.size.y, 0.1f, 10000.0f);
        }
        if (ImGui::DragFloat("Move Speed", &viewport.getCamera().moveSpeed, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}
        if (ImGui::DragFloat("Move Constant", &viewport.getCamera().moveConstant, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}
        if (ImGui::DragFloat("Look Speed", &viewport.getCamera().lookSpeed, 0.1f, 0.0001f, FLT_MAX, "%.4f")) {}
        if (ImGui::DragFloat("Look Constant", &viewport.getCamera().lookConstant, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}
        if (ImGui::DragFloat("Zoom Speed", &viewport.getCamera().zoomSpeed, 0.001f, 0.0001f, FLT_MAX, "%.4f")) {}
        if (ImGui::DragFloat("Zoom Constant", &viewport.getCamera().zoomConstant, 0.001f, 0.001f, FLT_MAX, "%.4f")) {}

        ImGui::End();

        // if the scene containt at least one model, AND the active model is pointing at a valid model,
        // AND the active model has a mesh to modify, the properties window draws
        if (active) {
            gizmo.drawWindow();
        }

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
        pos = ImGui::GetWindowPos();

        // determine if the mouse is hovering the viewport 
        if (ImGui::IsWindowHovered()) {
            mouseInViewport = true;
        } else {
            mouseInViewport = false;
        }

        if (io.MouseClicked[0] && mouseInViewport && !(active != NULL && ImGuizmo::IsOver())) {
            // get mouse position in window
            glm::ivec2 mousePosition;
            SDL_GetMouseState(&mousePosition.x, &mousePosition.y);

            // get mouse position relative to viewport
            glm::ivec2 rendererMousePosition = { (mousePosition.x - pos.x), (mousePosition.y - pos.y) };

            // flip mouse coords for opengl
            rendererMousePosition.y = viewport.size.y - rendererMousePosition.y;

            active = geometryBufferPass->pick(rendererMousePosition.x, rendererMousePosition.y);
        }

        // render the active screen texture to the view port as an imgui image
        ImGui::Image(activeScreenTexture->ImGuiID(), ImVec2((float)viewport.size.x, (float)viewport.size.y), { 0,1 }, { 1,0 });

        // draw the imguizmo at the center of the active entity
        if (active) {
            gizmo.drawGuizmo(newScene, viewport, active);
        }

        ImGui::End();
        ImGui::PopStyleVar();

        ImGui::End();
        Renderer::ImGuiRender();
        Renderer::SwapBuffers(doVsync);

        if (resizing) {
            // adjust the camera and gizmo
            viewport.getCamera().getProjection() = glm::perspectiveRH(glm::radians(fov), (float)viewport.size.x / (float)viewport.size.y, 0.1f, 10000.0f);
            ImGuizmo::SetRect(pos.x, pos.y, size.x, size.y);

            // resizing framebuffers
            bloomPass->resize(viewport);
            tonemappingPass->resize(viewport);
            geometryBufferPass->resize(viewport);
            ambientOcclusionPass->resize(viewport);
            lightingPass->resize(viewport);
            voxelDebugPass->resize(viewport);
            aabbDebugPass->resize(viewport);

            
            resizing = false;
        }

        deltaTimer.stop();
        deltaTime = deltaTimer.elapsedMs();

    } // while true loop

    display = SDL_GetWindowDisplayIndex(directxwindow);
    //clean up SDL
    SDL_DestroyWindow(directxwindow);
    SDL_Quit();

} // application::run

} // namespace Raekor  