#include "pch.h"
#include "app.h"
#include "util.h"
#include "scene.h"
#include "entry.h"
#include "camera.h"
#include "shader.h"
#include "framebuffer.h"
#include "PlatformContext.h"
#include "renderer.h"
#include "buffer.h"
#include "timer.h"
#include "renderpass.h"

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
    auto context = Raekor::PlatformContext();

    // retrieve the application settings from the config file
    serializeSettings("config.json");

    int sdlError = SDL_Init(SDL_INIT_VIDEO);
    m_assert(sdlError == 0, "failed to init SDL for video");

    Uint32 wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL |
        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED;

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

    // initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // create the renderer object that does sets up the API and does all the runtime magic
    Renderer::setAPI(RenderAPI::OPENGL);
    Renderer::Init(directxwindow);

    // load the model files listed in the project section of config.json
    // basically acts like a budget project file
    Scene scene;
    std::vector<SceneObject>::iterator activeObject = scene.objects.end();
    Timer timer;
    timer.start();
    for (const std::string& path : project) {
        scene.add(path);
    }
    timer.stop();
    std::cout << "Setup time = " << timer.elapsedMs() << '\n';

    if (!scene.objects.empty()) {
        activeObject = scene.objects.begin();
    }

    std::unique_ptr<Mesh> cubeMesh;
    std::unique_ptr<glTextureCube> skyboxCubemap;

    Ffilter meshFileFormats;
    meshFileFormats.name = "Supported Mesh Files";
    meshFileFormats.extensions = "*.obj;*.fbx;*.gltf;*.glb";

    Ffilter textureFileFormats;
    textureFileFormats.name = "Supported Image Files";
    textureFileFormats.extensions = "*.png;*.jpg;*.jpeg;*.tga";

    std::unique_ptr<glShader> skyboxShader;
    std::vector<Shader::Stage> skyboxStages;
    skyboxStages.emplace_back(Shader::Type::VERTEX,   "shaders\\OpenGL\\skybox.vert");
    skyboxStages.emplace_back(Shader::Type::FRAG,     "shaders\\OpenGL\\skybox.frag");
    skyboxShader.reset(new glShader(skyboxStages.data(), skyboxStages.size()));

    std::unique_ptr<glShader> quadShader;
    std::vector<Shader::Stage> quadStages;
    quadStages.emplace_back(Shader::Type::VERTEX,     "shaders\\OpenGL\\quad.vert");
    quadStages.emplace_back(Shader::Type::FRAG,       "shaders\\OpenGL\\quad.frag");
    quadShader.reset(new glShader(quadStages.data(), quadStages.size())); 

    std::unique_ptr<glShader> voxelShader;
    std::vector<Shader::Stage> voxelStages;
    voxelStages.emplace_back(Shader::Type::VERTEX,  "shaders\\OpenGL\\voxelize.vert");
    voxelStages.emplace_back(Shader::Type::GEO,     "shaders\\OpenGL\\voxelize.geom");
    voxelStages.emplace_back(Shader::Type::FRAG,    "shaders\\OpenGL\\voxelize.frag");
    voxelShader.reset(new glShader(voxelStages.data(), voxelStages.size()));


    std::unique_ptr<glShader> basicShader;
    std::vector<Shader::Stage> basicStages;
    basicStages.emplace_back(Shader::Type::VERTEX,  "shaders\\OpenGL\\basic.vert");
    basicStages.emplace_back(Shader::Type::FRAG,    "shaders\\OpenGL\\basic.frag");
    basicShader.reset(new glShader(basicStages.data(), basicStages.size()));


    std::unique_ptr<glShader> voxelDebugShader;
    std::vector<Shader::Stage> voxelDebugStages;
    voxelDebugStages.emplace_back(Shader::Type::VERTEX,     "shaders\\OpenGL\\voxelDebug.vert");
    voxelDebugStages.emplace_back(Shader::Type::FRAG,   "shaders\\OpenGL\\voxelDebug.frag");
    voxelDebugShader.reset(new glShader(voxelDebugStages.data(), voxelDebugStages.size()));


    std::unique_ptr<glShader> cubemapDebugShader;
    std::vector<Shader::Stage> cubemapDebugStages;
    cubemapDebugStages.emplace_back(Shader::Type::VERTEX,    "shaders\\OpenGL\\simple.vert");
    cubemapDebugStages.emplace_back(Shader::Type::FRAG,      "shaders\\OpenGL\\simple.frag");
    cubemapDebugShader.reset(new glShader(cubemapDebugStages.data(), cubemapDebugStages.size()));

    cubeMesh.reset(new Mesh(Shape::Cube));
    cubeMesh->getVertexBuffer()->setLayout({
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

    uint32_t renderWidth = static_cast<uint32_t>(displays[display].w * .8f); 
    uint32_t renderHeight = static_cast<uint32_t>(displays[display].h * .8f);
    
    renderWidth = 2003;
    renderHeight = 1370;

    constexpr unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;

    // Generate texture on GPU.
    unsigned int voxelTexture;
    glGenTextures(1, &voxelTexture);
    glBindTexture(GL_TEXTURE_3D, voxelTexture);

    // Parameter options.
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Upload texture buffer.
    glTexStorage3D(GL_TEXTURE_3D, 7, GL_RGBA32F, 512, 512, 512);
    GLfloat clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    glClearTexImage(voxelTexture, 0, GL_RGBA, GL_FLOAT, &clearColor);
    glGenerateMipmap(GL_TEXTURE_3D);
    glBindTexture(GL_TEXTURE_3D, 0);

    // 3D texture visualization r esources
    //
    glTexture2D cubeBackfaceTexture;
    cubeBackfaceTexture.bind();
    cubeBackfaceTexture.init(renderWidth, renderHeight, Format::RGBA_F);
    cubeBackfaceTexture.setFilter(Sampling::Filter::None);
    cubeBackfaceTexture.unbind();

    glTexture2D cubeFrontfaceTexture;
    cubeFrontfaceTexture.bind();
    cubeFrontfaceTexture.init(renderWidth, renderHeight, Format::RGBA_F);
    cubeFrontfaceTexture.setFilter(Sampling::Filter::None);
    cubeFrontfaceTexture.unbind();

    glRenderbuffer cubeTexture;
    cubeTexture.init(renderWidth, renderHeight, GL_DEPTH32F_STENCIL8);
       
    glFramebuffer cubeBackfaceFramebuffer;
    cubeBackfaceFramebuffer.bind();
    cubeBackfaceFramebuffer.attach(cubeBackfaceTexture, GL_COLOR_ATTACHMENT0);
    cubeBackfaceFramebuffer.unbind();

    glFramebuffer cubeFrontfaceFramebuffer;
    cubeFrontfaceFramebuffer.bind();
    cubeFrontfaceFramebuffer.attach(cubeFrontfaceTexture, GL_COLOR_ATTACHMENT0);
    cubeFrontfaceFramebuffer.unbind();

    glTexture2D voxelVisTexture;
    voxelVisTexture.bind();
    voxelVisTexture.init(renderWidth, renderHeight, Format::RGBA_F16);
    voxelVisTexture.setFilter(Sampling::Filter::None);
    voxelVisTexture.unbind();

    glFramebuffer voxelVisFramebuffer;
    voxelVisFramebuffer.bind();
    voxelVisFramebuffer.attach(voxelVisTexture, GL_COLOR_ATTACHMENT0);
    voxelVisFramebuffer.unbind();

    // persistent imgui variable values
    auto activeSkyboxTexture = skyboxes.find("lake");

    SDL_SetWindowInputFocus(directxwindow);

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

    // timer for keeping frametime
    Timer deltaTimer;
    double deltaTime = 0;

    // setup  light matrices for a movable point light
    glm::mat4 lightmatrix = glm::mat4(1.0f);
    scene.pointLight.position = { 0.0f, 1.8f, 0.0f };
    lightmatrix = glm::translate(lightmatrix, scene.pointLight.position);
    float lightPos[3], lightRot[3], lightScale[3];
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(lightmatrix), lightPos, lightRot, lightScale);

    // setup the camera that acts as the sun's view (directional light)
    glm::vec2 planes = { 1.0, 20.0f };
    float orthoSize = 16.0f;
    scene.sunCamera.getProjection() = glm::orthoRH_ZO(-orthoSize, orthoSize, -orthoSize, orthoSize, planes.x, planes.y);


    // voxel pass
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    voxelShader->bind();

    glViewport(0, 0, 512, 512);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glBindImageTexture(1, voxelTexture, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    voxelShader->getUniform("view") = scene.camera.getView();
    voxelShader->getUniform("projection") = scene.camera.getProjection();

    for (auto& object : scene) {
        voxelShader->getUniform("model") = object.transform;
        object.render();
    }

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    // all render passes
    auto bloomPass = std::make_unique<RenderPass::Bloom>(renderWidth, renderHeight);
    auto lightingPass = std::make_unique<RenderPass::DeferredLighting>(renderWidth, renderHeight);
    auto shadowMapPass = std::make_unique<RenderPass::ShadowMap>(SHADOW_WIDTH, SHADOW_HEIGHT);
    auto tonemappingPass = std::make_unique<RenderPass::Tonemapping>(renderWidth, renderHeight);
    auto omniShadowMapPass = std::make_unique<RenderPass::OmniShadowMap>(SHADOW_WIDTH, SHADOW_HEIGHT);
    auto geometryBufferPass = std::make_unique<RenderPass::GeometryBuffer>(renderWidth, renderHeight);
    auto ambientOcclusionPass = std::make_unique<RenderPass::ScreenSpaceAmbientOcclusion>(renderWidth, renderHeight);

    // boolean settings needed for a couple things
    bool mouseInViewport = false, gizmoEnabled = false, showSettingsWindow = false;
    bool doSSAO = true, doBloom = false;

    // keep a pointer to the texture that's rendered to the window
    glTexture2D* activeScreenTexture = &tonemappingPass->result;

    while (running) {
        deltaTimer.start();
        handleEvents(directxwindow, scene.camera, mouseInViewport, deltaTime);
        scene.sunCamera.update(true);
        scene.camera.update(true);

        // clear the main window
        Renderer::Clear({ 0.22f, 0.32f, 0.42f, 1.0f });

        // voxel visualization pass
        //
        glViewport(0, 0, renderWidth, renderHeight);
        glCullFace(GL_BACK);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        cubeBackfaceFramebuffer.bind();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        basicShader->bind();
        basicShader->getUniform("projection") = scene.camera.getProjection();
        basicShader->getUniform("view") = scene.camera.getView();
        basicShader->getUniform("model") = glm::mat4(1.0f);

        cubeMesh->render();

        glCullFace(GL_FRONT);
        cubeFrontfaceFramebuffer.bind();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        cubeMesh->render();

        // render to screen
        glDisable(GL_CULL_FACE);
        voxelDebugShader->bind();
        voxelVisFramebuffer.bind();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        cubeBackfaceTexture.bindToSlot(0);
        cubeFrontfaceTexture.bindToSlot(1);
        glBindTextureUnit(2, voxelTexture);
        voxelDebugShader->getUniform("cameraPosition") = scene.camera.getPosition();

        Quad->render();

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        //
        // end voxel visualization pass

        // generate sun shadow map 
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        shadowMapPass->execute(scene, scene.sunCamera);

        // generate point light shadow map 
        omniShadowMapPass->execute(scene, glm::make_vec3(lightPos));

        // generate a geometry buffer
        glViewport(0, 0, renderWidth, renderHeight);
        geometryBufferPass->execute(scene);

        if (doSSAO) {
            ambientOcclusionPass->execute(scene, geometryBufferPass.get(), Quad.get());
        }

        // perform deferred lighting pass
        glViewport(0, 0, renderWidth, renderHeight);
        lightingPass->execute(
            scene, 
            shadowMapPass.get(), 
            omniShadowMapPass.get(),
            geometryBufferPass.get(),
            ambientOcclusionPass.get(),
            Quad.get()
        );

        // perform Bloom
        if (doBloom) {
            bloomPass->execute(lightingPass->result, lightingPass->bloomHighlights, Quad.get());
        }

        if (doBloom) {
            tonemappingPass->execute(bloomPass->result, Quad.get());
        } else {
            tonemappingPass->execute(lightingPass->result, Quad.get());
        }

        //get new frame for ImGui and ImGuizmo
        Renderer::ImGuiNewFrame(directxwindow);
        ImGuizmo::BeginFrame();

        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags dockWindowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
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
        if ((lightPos[0] >= bounds && lightMoveSpeed > 0) || (lightPos[0] <= -bounds && lightMoveSpeed < 0)) {
            lightMoveSpeed *= -1;
        }
        if (moveLight) {
            lightmatrix = glm::translate(lightmatrix, { lightMoveAmount, 0.0, 0.0 });
        }

        // draw the top user bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Load model")) {
                    std::string path = context.openFileDialog({ meshFileFormats });
                    if (!path.empty()) {
                        project.push_back(path);
                        scene.add(path);
                    }
                }
                if (ImGui::MenuItem("Save project", "CTRL + S")) {
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
                    if (activeObject != scene.objects.end()) {
                        scene.erase(activeObject->name);
                        activeObject = scene.objects.begin();
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete), true)) {
                // on press we remove the scene object
                if (activeObject != scene.objects.end()) {
                    scene.erase(activeObject->name);
                    activeObject = scene.objects.begin();
                }
            }

            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About", "")) {}
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // model panel
        ImGui::Begin("Entities");
        if (ImGui::IsWindowFocused()) {
            if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete), false)) {}
        }
        
        auto treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen;
        if (ImGui::TreeNodeEx("Objects", treeNodeFlags)) {
            ImGui::Columns(1, NULL, false);
            // draw a selectable for every object in the scene
            unsigned int uniqueID = 0;
            for (auto& object : scene.objects) {
                    bool selected = activeObject->name == object.name;
                    if (ImGui::Selectable(object.name.c_str(), selected)) {
                        activeObject = scene.at(object.name);
                    }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
                uniqueID = uniqueID + 1;
            }
            ImGui::TreePop();
        }

        ImGui::End();

        // post processing panel
        ImGui::Begin("Post Processing");
        static bool doTonemapping = true;
        if (ImGui::Checkbox("HDR", &doTonemapping)) {
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
            if (!doSSAO) ambientOcclusionPass->result.clear({ 1.0f, 1.0f, 1.0f });
        }
        ImGui::Separator();
        if (ImGui::DragFloat(   "Samples",  &ambientOcclusionPass->settings.samples, 8.0f, 8.0f, 64.0f)) {}
        if (ImGui::SliderFloat( "Power",    &ambientOcclusionPass->settings.power, 0.0f, 15.0f)) {}
        if (ImGui::SliderFloat( "Bias",     &ambientOcclusionPass->settings.bias, 0.0f, 1.0f)) {}

        ImGui::End();

        // scene panel
        ImGui::Begin("Scene");
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
            if (ImGui::Selectable(nameof(voxelVisTexture), activeScreenTexture->ImGuiID() == voxelVisTexture.ImGuiID()))
                activeScreenTexture = &voxelVisTexture;
            if (ImGui::Selectable(nameof(cubeFrontfaceTexture), activeScreenTexture->ImGuiID() == cubeFrontfaceTexture.ImGuiID()))
                activeScreenTexture = &cubeFrontfaceTexture;
            if (ImGui::Selectable(nameof(cubeBackfaceTexture), activeScreenTexture->ImGuiID() == cubeBackfaceTexture.ImGuiID()))
                activeScreenTexture = &cubeBackfaceTexture;

            ImGui::TreePop();
        }

        ImGui::NewLine();

        ImGui::Text("Directional Light");
        ImGui::Separator();
        if (ImGui::DragFloat2("Angle", glm::value_ptr(scene.sunCamera.getAngle()), 0.01f)) {}
        
        if (ImGui::DragFloat2("Planes", glm::value_ptr(planes), 0.1f)) {
            scene.sunCamera.getProjection() = glm::orthoRH_ZO(-orthoSize, orthoSize, -orthoSize, orthoSize, planes.x, planes.y);
        }
        if (ImGui::DragFloat("Size", &orthoSize)) {
            scene.sunCamera.getProjection() = glm::orthoRH_ZO(-orthoSize, orthoSize, -orthoSize, orthoSize, planes.x, planes.y);
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

        ImGui::ShowMetricsWindow();

        ImGui::Begin("Camera Properties");
        static float fov = 45.0f;
        if (ImGui::DragFloat("FoV", &fov, 1.0f, 35.0f, 120.0f)) {
            scene.camera.getProjection() = glm::perspectiveRH(glm::radians(fov), (float)renderWidth / (float)renderHeight, 0.1f, 100.0f);
        }
        if (ImGui::DragFloat("Move Speed", &scene.camera.moveSpeed, 0.001f, 0.001f, FLT_MAX, "%.3f")) {}
        if (ImGui::DragFloat("Look Speed", &scene.camera.lookSpeed, 0.0001f, 0.0001f, FLT_MAX, "%.4f")) {}
        ImGui::End();

        // if the scene containt at least one model, AND the active model is pointing at a valid model,
        // AND the active model has a mesh to modify, the properties window draws
        static ImGuizmo::OPERATION operation = ImGuizmo::OPERATION::TRANSLATE;
        if (!scene.objects.empty() && activeObject != scene.objects.end()) {
            ImGui::Begin("Editor");

            if(ImGui::Checkbox("Gizmo", &gizmoEnabled)) {
                ImGuizmo::Enable(gizmoEnabled);
            }

            ImGui::Separator();

            std::array<const char*, 3> previews = {
                "TRANSLATE", "ROTATE", "SCALE"
            };

            if (ImGui::BeginCombo("Mode", previews[operation])) {
                for (int i = 0; i < previews.size(); i++) {
                    bool selected = (i == operation);
                    if (ImGui::Selectable(previews[i], selected)) {
                        operation = (ImGuizmo::OPERATION)i;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            // resets the model's transformation
            if (ImGui::Button("Reset")) {
                activeObject->reset();
            }

            ImGui::End();
        }

        // renderer viewport
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Renderer", NULL, ImGuiWindowFlags_AlwaysAutoResize);

        // figure out if we need to resize the viewport
        static bool resizing = false;
        auto size = ImGui::GetContentRegionAvail();
        if (renderWidth != size.x || renderHeight != size.y) {
            resizing = true;
            renderWidth = static_cast<uint32_t>(size.x), renderHeight = static_cast<uint32_t>(size.y);
        }
        auto pos = ImGui::GetWindowPos();

        // determine if the mouse is hovering the 
        if (ImGui::IsWindowHovered()) {
            mouseInViewport = true;
        } else {
            mouseInViewport = false;
        }

        // render the active screen texture to the view port as an imgui image
        ImGui::Image(activeScreenTexture->ImGuiID(), ImVec2((float)renderWidth, (float)renderHeight), { 0,1 }, { 1,0 });

        // draw the imguizmo at the center of the light
        if (gizmoEnabled) {
            ImGuizmo::SetDrawlist();
            auto gizmoData = moveLight ? glm::value_ptr(activeObject->transform) : glm::value_ptr(lightmatrix);
            ImGuizmo::Manipulate(glm::value_ptr(scene.camera.getView()), glm::value_ptr(scene.camera.getProjection()), operation, ImGuizmo::MODE::WORLD, gizmoData);
        }

        ImGui::End();
        ImGui::PopStyleVar();

        ImGui::End();
        Renderer::ImGuiRender();
        Renderer::SwapBuffers(doVsync);

        if (resizing) {
            // adjust the camera and gizmo
            scene.camera.getProjection() = glm::perspectiveRH(glm::radians(fov), (float)renderWidth / (float)renderHeight, 0.1f, 100.0f);
            ImGuizmo::SetRect(pos.x, pos.y, size.x, size.y);

            // resizing framebuffers
            bloomPass->resize(renderWidth, renderHeight);
            tonemappingPass->resize(renderWidth, renderHeight);
            geometryBufferPass->resize(renderWidth, renderHeight);
            ambientOcclusionPass->resize(renderWidth, renderHeight);
            
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