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

#define nameof(var) (#var)

namespace Raekor {

struct ShadowMapUniforms {
    glm::mat4 cameraMatrix;
};

struct LightPassUniforms {
    glm::mat4 view, projection;
    glm::mat4 lightSpaceMatrix;
    glm::vec4 DirViewPos;
    glm::vec4 DirLightPos;
    glm::vec4 pointLightPos;
    unsigned int renderFlags = 0b00000001;
};

struct DirLightUniforms {
    glm::vec4 sunColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    float minBias = 0.000f, maxBias = 0.0f;
    float farPlane = 25.0f;
};

struct TonemapUniforms {
    float exposure = 1.0f;
    float gamma = 1.8f;
};

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

    LightPassUniforms lightUniforms = {};
    ShadowMapUniforms shadowMapUniforms = {};
    TonemapUniforms tonemapUniforms = {};
    DirLightUniforms uniforms = {};

    std::unique_ptr<GLResourceBuffer> lightResourceBuffer;
    lightResourceBuffer.reset(new GLResourceBuffer(sizeof(LightPassUniforms)));

    std::unique_ptr<GLResourceBuffer> shadowMapResourceBuffer;
    shadowMapResourceBuffer.reset(new GLResourceBuffer(sizeof(ShadowMapUniforms)));

    std::unique_ptr<GLResourceBuffer> tonemapResourceBuffer;
    tonemapResourceBuffer.reset(new GLResourceBuffer(sizeof(TonemapUniforms)));


    std::unique_ptr<Mesh> cubeMesh;
    std::unique_ptr<glTextureCube> skyboxCubemap;

    Ffilter meshFileFormats;
    meshFileFormats.name = "Supported Mesh Files";
    meshFileFormats.extensions = "*.obj;*.fbx;*.gltf;*.glb";

    Ffilter textureFileFormats;
    textureFileFormats.name = "Supported Image Files";
    textureFileFormats.extensions = "*.png;*.jpg;*.jpeg;*.tga";


    std::unique_ptr<GLShader> mainShader;
    Shader::Stage vertex(Shader::Type::VERTEX, "shaders\\OpenGL\\main.vert");
    Shader::Stage frag(Shader::Type::FRAG, "shaders\\OpenGL\\main.frag");
    std::array<Shader::Stage, 2> modelStages = { vertex, frag };
    mainShader.reset(new GLShader(modelStages.data(), modelStages.size()));

    std::unique_ptr<GLShader> skyboxShader;
    std::vector<Shader::Stage> skyboxStages;
    skyboxStages.emplace_back(Shader::Type::VERTEX,   "shaders\\OpenGL\\skybox.vert");
    skyboxStages.emplace_back(Shader::Type::FRAG,     "shaders\\OpenGL\\skybox.frag");
    skyboxShader.reset(new GLShader(skyboxStages.data(), skyboxStages.size()));

    std::unique_ptr<GLShader> shadowmapShader;
    std::vector<Shader::Stage> shadowmapStages;
    shadowmapStages.emplace_back(Shader::Type::VERTEX,    "shaders\\OpenGL\\depth.vert");
    shadowmapStages.emplace_back(Shader::Type::FRAG,      "shaders\\OpenGL\\depth.frag");
    shadowmapShader.reset(new GLShader(shadowmapStages.data(), shadowmapStages.size()));


    std::unique_ptr<GLShader> omniShadowmapShader;
    std::vector<Shader::Stage> omniShadowmapStages;
    omniShadowmapStages.emplace_back(Shader::Type::VERTEX,    "shaders\\OpenGL\\depthCube.vert");
    omniShadowmapStages.emplace_back(Shader::Type::FRAG,      "shaders\\OpenGL\\depthCube.frag");
    omniShadowmapShader.reset(new GLShader(omniShadowmapStages.data(), omniShadowmapStages.size()));


    std::unique_ptr<GLShader> quadShader;
    std::vector<Shader::Stage> quadStages;
    quadStages.emplace_back(Shader::Type::VERTEX,     "shaders\\OpenGL\\quad.vert");
    quadStages.emplace_back(Shader::Type::FRAG,       "shaders\\OpenGL\\quad.frag");
    quadShader.reset(new GLShader(quadStages.data(), quadStages.size()));


    std::unique_ptr<GLShader> blurShader;
    std::vector<Shader::Stage> blurStages;
    blurStages.emplace_back(Shader::Type::VERTEX,     "shaders\\OpenGL\\quad.vert");
    blurStages.emplace_back(Shader::Type::FRAG,       "shaders\\OpenGL\\gaussian.frag");
    blurShader.reset(new GLShader(blurStages.data(), blurStages.size()));


    std::unique_ptr<GLShader> bloomShader;
    std::vector<Shader::Stage> bloomStages;
    bloomStages.emplace_back(Shader::Type::VERTEX,    "shaders\\OpenGL\\quad.vert");
    bloomStages.emplace_back(Shader::Type::FRAG,      "shaders\\OpenGL\\bloom.frag");
    bloomShader.reset(new GLShader(bloomStages.data(), bloomStages.size()));
    

    std::unique_ptr<GLShader> voxelShader;
    std::vector<Shader::Stage> voxelStages;
    voxelStages.emplace_back(Shader::Type::VERTEX,  "shaders\\OpenGL\\voxelize.vert");
    voxelStages.emplace_back(Shader::Type::GEO,     "shaders\\OpenGL\\voxelize.geom");
    voxelStages.emplace_back(Shader::Type::FRAG,    "shaders\\OpenGL\\voxelize.frag");
    voxelShader.reset(new GLShader(voxelStages.data(), voxelStages.size()));


    std::unique_ptr<GLShader> basicShader;
    std::vector<Shader::Stage> basicStages;
    basicStages.emplace_back(Shader::Type::VERTEX,  "shaders\\OpenGL\\basic.vert");
    basicStages.emplace_back(Shader::Type::FRAG,    "shaders\\OpenGL\\basic.frag");
    basicShader.reset(new GLShader(basicStages.data(), basicStages.size()));


    std::unique_ptr<GLShader> voxelDebugShader;
    std::vector<Shader::Stage> voxelDebugStages;
    voxelDebugStages.emplace_back(Shader::Type::VERTEX,     "shaders\\OpenGL\\voxelDebug.vert");
    voxelDebugStages.emplace_back(Shader::Type::FRAG,   "shaders\\OpenGL\\voxelDebug.frag");
    voxelDebugShader.reset(new GLShader(voxelDebugStages.data(), voxelDebugStages.size()));


    std::unique_ptr<GLShader> cubemapDebugShader;
    std::vector<Shader::Stage> cubemapDebugStages;
    cubemapDebugStages.emplace_back(Shader::Type::VERTEX,    "shaders\\OpenGL\\simple.vert");
    cubemapDebugStages.emplace_back(Shader::Type::FRAG,      "shaders\\OpenGL\\simple.frag");
    cubemapDebugShader.reset(new GLShader(cubemapDebugStages.data(), cubemapDebugStages.size()));


    std::unique_ptr<GLShader> ssaoShader;
    std::vector<Shader::Stage> ssaoStages;
    ssaoStages.emplace_back(Shader::Type::VERTEX,     "shaders\\OpenGL\\SSAO.vert");
    ssaoStages.emplace_back(Shader::Type::FRAG,       "shaders\\OpenGL\\SSAO.frag");
    ssaoShader.reset(new GLShader(ssaoStages.data(), ssaoStages.size()));


    std::unique_ptr<GLShader> gbufferShader;
    std::vector<Shader::Stage> gbufferStages;
    gbufferStages.emplace_back(Shader::Type::VERTEX,  "shaders\\OpenGL\\gbuffer.vert");
    gbufferStages.emplace_back(Shader::Type::FRAG,    "shaders\\OpenGL\\gbuffer.frag");
    gbufferStages[0].defines = { "NO_NORMAL_MAP" };
    gbufferStages[1].defines = { "NO_NORMAL_MAP" };
    gbufferShader.reset(new GLShader(gbufferStages.data(), gbufferStages.size()));


    std::unique_ptr<GLShader> ssaoBlurShader;
    std::vector<Shader::Stage> ssaoBlurStages;
    ssaoBlurStages.emplace_back(Shader::Type::VERTEX,     "shaders\\OpenGL\\quad.vert");
    ssaoBlurStages.emplace_back(Shader::Type::FRAG,       "shaders\\OpenGL\\SSAOblur.frag");
    ssaoBlurShader.reset(new GLShader(ssaoBlurStages.data(), ssaoBlurStages.size()));
    

    std::unique_ptr<GLShader> tonemapShader;
    std::vector<Shader::Stage> tonemapStages;
    std::vector<std::string> tonemappingShaders = {
       "shaders\\OpenGL\\HDR.frag",
       "shaders\\OpenGL\\HDRreinhard.frag",
       "shaders\\OpenGL\\HDRuncharted.frag"
    };
    tonemapStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\HDR.vert");
    tonemapStages.emplace_back(Shader::Type::FRAG, tonemappingShaders.begin()->c_str());
    tonemapShader.reset(new GLShader(tonemapStages.data(), tonemapStages.size()));

    ShaderHotloader hotloader;
    hotloader.watch(mainShader.get(), modelStages.data(), modelStages.size());
    hotloader.watch(gbufferShader.get(), gbufferStages.data(), gbufferStages.size());


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

    glTexture2D albedoTexture, normalTexture, positionTexture;
    
    albedoTexture.bind();
    albedoTexture.init(renderWidth, renderHeight, Format::RGBA_F16);
    albedoTexture.setFilter(Sampling::Filter::None);
    albedoTexture.unbind();

    normalTexture.bind();
    normalTexture.init(renderWidth, renderHeight, Format::RGBA_F16);
    normalTexture.setFilter(Sampling::Filter::None);
    normalTexture.unbind();

    positionTexture.bind();
    positionTexture.init(renderWidth, renderHeight, Format::RGBA_F16);
    positionTexture.setFilter(Sampling::Filter::None);
    positionTexture.setWrap(Sampling::Wrap::ClampEdge);
    positionTexture.unbind();

    glRenderbuffer GDepthBuffer;
    GDepthBuffer.init(renderWidth, renderHeight, GL_DEPTH32F_STENCIL8);

    glFramebuffer GBuffer;
    GBuffer.bind();
    GBuffer.attach(positionTexture,     GL_COLOR_ATTACHMENT0);
    GBuffer.attach(normalTexture,       GL_COLOR_ATTACHMENT1);
    GBuffer.attach(albedoTexture,       GL_COLOR_ATTACHMENT2);
    GBuffer.attach(GDepthBuffer,        GL_DEPTH_STENCIL_ATTACHMENT);
    GBuffer.unbind();


    glTexture2D preprocessTexture;
    preprocessTexture.bind();
    preprocessTexture.init(renderWidth, renderHeight, Format::RGBA_F16);
    preprocessTexture.setFilter(Sampling::Filter::Bilinear);
    preprocessTexture.unbind();

    glFramebuffer preprocessFramebuffer;
    preprocessFramebuffer.bind();
    preprocessFramebuffer.attach(preprocessTexture, GL_COLOR_ATTACHMENT0);
    preprocessFramebuffer.unbind();

    glTexture2D hdrTexture;
    hdrTexture.bind();
    hdrTexture.init(renderWidth, renderHeight, Format::RGBA_F16);
    hdrTexture.setFilter(Sampling::Filter::Bilinear);
    hdrTexture.unbind();

    glTexture2D bloomTexture;
    bloomTexture.bind();
    bloomTexture.init(renderWidth, renderHeight, Format::RGBA_F16);
    bloomTexture.setFilter(Sampling::Filter::Bilinear);
    bloomTexture.unbind();

    glRenderbuffer hdrRenderbuffer;
    hdrRenderbuffer.init(renderWidth, renderHeight, GL_DEPTH32F_STENCIL8);

    glFramebuffer hdrFramebuffer;
    hdrFramebuffer.bind();
    hdrFramebuffer.attach(hdrTexture, GL_COLOR_ATTACHMENT0);
    hdrFramebuffer.attach(bloomTexture, GL_COLOR_ATTACHMENT1);
    hdrFramebuffer.attach(hdrRenderbuffer, GL_DEPTH_STENCIL_ATTACHMENT);
    hdrFramebuffer.unbind();

    glTexture2D finalTexture;
    finalTexture.bind();
    finalTexture.init(renderWidth, renderHeight, Format::RGB_F);
    finalTexture.setFilter(Sampling::Filter::Bilinear);
    finalTexture.unbind();

    glRenderbuffer finalRenderbuffer;
    finalRenderbuffer.init(renderWidth, renderHeight, GL_DEPTH32F_STENCIL8);

    glFramebuffer finalFramebuffer;
    finalFramebuffer.bind();
    finalFramebuffer.attach(finalTexture, GL_COLOR_ATTACHMENT0);
    finalFramebuffer.attach(finalRenderbuffer, GL_DEPTH_STENCIL_ATTACHMENT);
    finalFramebuffer.unbind();

    glTexture2D pingpongTextures[2];
    glFramebuffer pingpongFramebuffers[2];

    for (unsigned int i = 0; i < 2; i++) {
        pingpongTextures[i].bind();
        pingpongTextures[i].init(renderWidth, renderHeight, Format::RGBA_F16);   
        pingpongTextures[i].setFilter(Sampling::Filter::Bilinear);
        pingpongTextures[i].setWrap(Sampling::Wrap::ClampEdge);
        pingpongTextures[i].unbind();

        pingpongFramebuffers[i].bind();
        pingpongFramebuffers[i].attach(pingpongTextures[i], GL_COLOR_ATTACHMENT0);
        pingpongFramebuffers[i].unbind();
    }

    // Generate texture on GPU.
    unsigned int voxelTexture;
    glGenTextures(1, &voxelTexture);
    glBindTexture(GL_TEXTURE_3D, voxelTexture);

    // Parameter options.
    const auto wrap = GL_CLAMP_TO_BORDER;
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, wrap);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, wrap);

    const auto filter = GL_LINEAR_MIPMAP_LINEAR;
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Upload texture buffer.
    const int levels = 7;
    glTexStorage3D(GL_TEXTURE_3D, levels, GL_RGBA32F, 512, 512, 512);
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

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigDockingWithShift = true;
    ImFont* pFont = io.Fonts->AddFontFromFileTTF(font.c_str(), 15.0f);
    if (!io.Fonts->Fonts.empty()) {
        io.FontDefault = io.Fonts->Fonts.back();
    }

    ImVec4* colors = ImGui::GetStyle().Colors;
    for (unsigned int i = 0; i < themeColors.size(); i++) {
        auto& savedColor = themeColors[i];
        colors[i] = ImVec4(savedColor[0], savedColor[1], savedColor[2], savedColor[3]);
    }


    Timer deltaTimer;
    double deltaTime = 0;

    float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };

    glTexture2D shadowTexture;
    shadowTexture.bind();
    shadowTexture.init(SHADOW_WIDTH, SHADOW_HEIGHT, Format::DEPTH);
    shadowTexture.setFilter(Sampling::Filter::None);
    shadowTexture.setWrap(Sampling::Wrap::ClampBorder);

    glFramebuffer shadowFramebuffer;
    shadowFramebuffer.bind();
    shadowFramebuffer.attach(shadowTexture, GL_DEPTH_ATTACHMENT);
    shadowFramebuffer.unbind();


    glTextureCube depthCubeTexture;
    depthCubeTexture.bind();
    for (unsigned int i = 0; i < 6; ++i) {
        depthCubeTexture.init(SHADOW_WIDTH, SHADOW_HEIGHT, i, Format::DEPTH, nullptr);
    }
    depthCubeTexture.setFilter(Sampling::Filter::None);
    depthCubeTexture.setWrap(Sampling::Wrap::ClampEdge);

    glFramebuffer depthCubeFramebuffer;
    depthCubeFramebuffer.bind();
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    depthCubeFramebuffer.unbind();

    // setup  light matrices for a movable point light
    glm::mat4 lightmatrix = glm::mat4(1.0f);
    lightmatrix = glm::translate(lightmatrix, { 0.0f, 1.8f, 0.0f });
    float lightPos[3], lightRot[3], lightScale[3];
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(lightmatrix), lightPos, lightRot, lightScale);

    // setup the camera that acts as the sun's view (directional light)
    glm::vec2 planes = { 1.0, 20.0f };
    float orthoSize = 16.0f;
    Camera sunCamera(glm::vec3(0, 15.0, 0), glm::orthoRH_ZO(-orthoSize, orthoSize, -orthoSize, orthoSize, planes.x, planes.y));
    sunCamera.getView() = glm::lookAtRH (
        glm::vec3(-2.0f, 12.0f, 2.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    sunCamera.getAngle().y = -1.325f;

    float nearPlane = 0.1f;
    float farPlane = 25.0f;
    glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), float(SHADOW_WIDTH / SHADOW_HEIGHT), nearPlane, farPlane);

    // create SSAO kernel hemisphere
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between 0.0 - 1.0
    std::default_random_engine generator;
    std::vector<glm::vec3> ssaoKernel;
    for (unsigned int i = 0; i < 64; ++i) {
        glm::vec3 sample(
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator)
        );
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        float scale = float(i / 64.0f);

        auto lerp = [](float a, float b, float f) {
            return a + f * (b - a);
        };

        scale = lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        ssaoKernel.push_back(sample);
    }

    std::vector<glm::vec3> ssaoNoise;
    for (unsigned int i = 0; i < 16; i++) {
        glm::vec3 noise(
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) * 2.0 - 1.0,
            0.0f);
        ssaoNoise.push_back(noise);
    }

    glTexture2D ssaoNoiseTexture;
    ssaoNoiseTexture.bind();
    ssaoNoiseTexture.init(4, 4, { GL_RGB16F, GL_RGB, GL_FLOAT }, &ssaoNoise[0]);
    ssaoNoiseTexture.setFilter(Sampling::Filter::None);
    ssaoNoiseTexture.setWrap(Sampling::Wrap::Repeat);

    glTexture2D ssaoColorTexture;
    ssaoColorTexture.bind();
    ssaoColorTexture.init(renderWidth, renderHeight, {GL_RGBA, GL_RGBA, GL_FLOAT}, nullptr);
    ssaoColorTexture.setFilter(Sampling::Filter::None);
 
    glFramebuffer ssaoFramebuffer;
    ssaoFramebuffer.bind();
    ssaoFramebuffer.attach(ssaoColorTexture, GL_COLOR_ATTACHMENT0);

    glTexture2D ssaoBlurTexture;
    ssaoBlurTexture.bind();
    ssaoBlurTexture.init(renderWidth, renderHeight, {GL_RGBA, GL_RGBA, GL_FLOAT}, nullptr);
    ssaoBlurTexture.setFilter(Sampling::Filter::None);

    glFramebuffer ssaoBlurFramebuffer;
    ssaoBlurFramebuffer.bind();
    ssaoBlurFramebuffer.attach(ssaoBlurTexture, GL_COLOR_ATTACHMENT0);

    float ssaoBias = 0.025f, ssaoPower = 2.5f;
    float ssaoSampleCount = 64.0f;

    glm::vec3 bloomThreshold { 2.0f, 2.0f, 2.0f };
    static bool doBloom = false;

    bool mouseInViewport = false;
    bool gizmoEnabled = false;
    bool showSettingsWindow = false;

    glTexture2D* activeScreenTexture = &finalTexture;

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

    while (running) {
        deltaTimer.start();
        handleEvents(directxwindow, scene.camera, mouseInViewport, deltaTime);
        sunCamera.update(true);
        scene.camera.update(true);

        hotloader.checkForUpdates();

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


        // setup the shadow map 
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        shadowFramebuffer.bind();
        glClear(GL_DEPTH_BUFFER_BIT);
        glCullFace(GL_FRONT);

        // render the entire scene to the directional light shadow map
        shadowmapShader->bind();
        shadowMapUniforms.cameraMatrix = sunCamera.getProjection() * sunCamera.getView();
        shadowMapResourceBuffer->update(&shadowMapUniforms, sizeof(shadowMapUniforms));
        shadowMapResourceBuffer->bind(0);

        for (auto& object : scene) {
            shadowmapShader->getUniform("model") = object.transform;
            object.render();
        }

        // setup the 3D shadow map 
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        depthCubeFramebuffer.bind();
        glCullFace(GL_BACK);

        // generate the view matrices for calculating lightspace
        std::vector<glm::mat4> shadowTransforms;
        glm::vec3 lightPosition = glm::make_vec3(lightPos);
        shadowTransforms.push_back(shadowProj * glm::lookAtRH(lightPosition, lightPosition + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
        shadowTransforms.push_back(shadowProj * glm::lookAtRH(lightPosition, lightPosition + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
        shadowTransforms.push_back(shadowProj * glm::lookAtRH(lightPosition, lightPosition + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)));
        shadowTransforms.push_back(shadowProj * glm::lookAtRH(lightPosition, lightPosition + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0)));
        shadowTransforms.push_back(shadowProj * glm::lookAtRH(lightPosition, lightPosition + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0)));
        shadowTransforms.push_back(shadowProj * glm::lookAtRH(lightPosition, lightPosition + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0)));

        // render every model using the depth cubemap shader
        omniShadowmapShader->bind();
        omniShadowmapShader->getUniform("farPlane") = farPlane;
        for (uint32_t i = 0; i < 6; i++) {
            depthCubeFramebuffer.attach(depthCubeTexture, GL_DEPTH_ATTACHMENT, i);
            glClear(GL_DEPTH_BUFFER_BIT);
            omniShadowmapShader->getUniform("projView") = shadowTransforms[i];
            omniShadowmapShader->getUniform("lightPos") = glm::make_vec3(lightPos);

            for (auto& object : scene) {
                omniShadowmapShader->getUniform("model") = object.transform;
                object.render();
            }
        }

        // start G-Buffer pass
        glViewport(0, 0, renderWidth, renderHeight);
        GBuffer.bind();

        gbufferShader->bind();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        gbufferShader->getUniform("projection") = scene.camera.getProjection();
        gbufferShader->getUniform("view") = scene.camera.getView();

        for (auto& object : scene) {
            gbufferShader->getUniform("model") = object.transform;
            object.render();
        }

        GBuffer.unbind();

        if (lightUniforms.renderFlags & 0x01) {
            // SSAO PASS
            ssaoFramebuffer.bind();
            positionTexture.bindToSlot(0);
            normalTexture.bindToSlot(1);
            ssaoNoiseTexture.bindToSlot(2);
            ssaoShader->bind();

            ssaoShader->getUniform("samples") = ssaoKernel;
            ssaoShader->getUniform("view") = scene.camera.getView();
            ssaoShader->getUniform("projection") = scene.camera.getProjection();
            ssaoShader->getUniform("noiseScale") = { renderWidth / 4.0f, renderHeight / 4.0f };
            ssaoShader->getUniform("sampleCount") = ssaoSampleCount;
            ssaoShader->getUniform("power") = ssaoPower;
            ssaoShader->getUniform("bias") = ssaoBias;

            Quad->render();

            // now blur the SSAO result
            ssaoBlurFramebuffer.bind();
            ssaoColorTexture.bindToSlot(0);
            ssaoBlurShader->bind();

            Quad->render();
        }

        // bind the main framebuffer
        hdrFramebuffer.bind();
        glViewport(0, 0, renderWidth, renderHeight);
        Renderer::Clear({ 0.0f, 0.0f, 0.0f, 1.0f });

        // set uniforms
        mainShader->bind();
        mainShader->getUniform("sunColor") = uniforms.sunColor;
        mainShader->getUniform("minBias") = uniforms.minBias;
        mainShader->getUniform("maxBias") = uniforms.maxBias;
        mainShader->getUniform("farPlane") = farPlane;
        mainShader->getUniform("bloomThreshold") = bloomThreshold;
        
        // bind textures to shader binding slots
        shadowTexture.bindToSlot(0);
        depthCubeTexture.bindToSlot(1);
        positionTexture.bindToSlot(2);
        albedoTexture.bindToSlot(3);
        normalTexture.bindToSlot(4);
        ssaoBlurTexture.bindToSlot(5);

        lightUniforms.view = scene.camera.getView();
        lightUniforms.projection = scene.camera.getProjection();
        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(lightmatrix), lightPos, lightRot, lightScale);
        lightUniforms.pointLightPos = glm::vec4(glm::make_vec3(lightPos), 1.0f);
        lightUniforms.DirLightPos = glm::vec4(sunCamera.getPosition(), 1.0);
        lightUniforms.DirViewPos = glm::vec4(scene.camera.getPosition(), 1.0);
        lightUniforms.lightSpaceMatrix = sunCamera.getProjection() * sunCamera.getView();

        lightResourceBuffer->update(&lightUniforms, sizeof(LightPassUniforms));
        lightResourceBuffer->bind(0);

        Quad->render();

        // unbind the main frame buffer
        hdrFramebuffer.unbind();

        // perform gaussian blur on the bloom texture using "ping pong" framebuffers that
        // take each others color attachments as input and perform a directional gaussian blur each
        // iteration
        bool horizontal = true, firstIteration = true;
        blurShader->bind();
        for (unsigned int i = 0; i < 10; i++) {
            pingpongFramebuffers[horizontal].bind();
            blurShader->getUniform("horizontal") = horizontal;
            if (firstIteration) {
                bloomTexture.bindToSlot(0);
                firstIteration = false;
            } else {
                pingpongTextures[!horizontal].bindToSlot(0);
            }
            Quad->render();
            horizontal = !horizontal;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // blend the bloom and scene texture together
        preprocessFramebuffer.bind();
        bloomShader->bind();
        hdrTexture.bindToSlot(0);
        pingpongTextures[!horizontal].bindToSlot(1);
        Quad->render();
        preprocessFramebuffer.unbind();

        
        static bool doTonemapping = true;
        if (doTonemapping) {
            finalFramebuffer.bind();
            glViewport(0, 0, renderWidth, renderHeight);
            tonemapShader->bind();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            tonemapResourceBuffer->update(&tonemapUniforms, sizeof(TonemapUniforms));
            tonemapResourceBuffer->bind(0);
            if (doBloom) {
                preprocessTexture.bindToSlot(0);
            } else {
                hdrTexture.bindToSlot(0);
            }
            Quad->render();
            finalFramebuffer.unbind();
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
        if (ImGui::Checkbox("HDR", &doTonemapping)) {
            if (doTonemapping) {
                activeScreenTexture = &finalTexture;
            } else {
                activeScreenTexture = &hdrTexture;
            }
        }
        ImGui::Separator();

        static const char* current = tonemappingShaders.begin()->c_str();
        if (ImGui::BeginCombo("Tonemapping", current)) {
            for (auto it = tonemappingShaders.begin(); it != tonemappingShaders.end(); it++) {
                bool selected = (it->c_str() == current);
                if (ImGui::Selectable(it->c_str(), selected)) {
                    current = it->c_str();

                    tonemapStages.clear();
                    tonemapStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\HDR.vert");
                    tonemapStages.emplace_back(Shader::Type::FRAG, current);
                    tonemapShader.reset(new GLShader(tonemapStages.data(), tonemapStages.size()));
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::SliderFloat("Exposure", &tonemapUniforms.exposure, 0.0f, 1.0f)) {}
        if (ImGui::SliderFloat("Gamma", &tonemapUniforms.gamma, 1.0f, 3.2f)) {}
        ImGui::NewLine();

        if (ImGui::Checkbox("Bloom", &doBloom)) {}
        ImGui::Separator();

        if (ImGui::DragFloat3("Threshold", glm::value_ptr(bloomThreshold), 0.001f, 0.0f, 10.0f)) {}
        ImGui::NewLine();

        if (ImGui::CheckboxFlags("SSAO", &lightUniforms.renderFlags, 0x01)) {}
        ImGui::Separator();
        if (ImGui::DragFloat("Samples", &ssaoSampleCount, 8.0f, 8.0f, 64.0f)) {}
        if (ImGui::SliderFloat("Power", &ssaoPower, 0.0f, 15.0f)) {}
        if (ImGui::SliderFloat("Bias", &ssaoBias, 0.0f, 1.0f)) {}


        ImGui::End();

        // scene panel
        ImGui::Begin("Scene");
        ImGui::SetItemDefaultFocus();

        if (ImGui::Button("Reload 'main' shaders")) {
            mainShader.reset(new GLShader(modelStages.data(), modelStages.size()));
        }

        // toggle button for openGl vsync
        static bool doVsync = false;
        if (ImGui::RadioButton("USE VSYNC", doVsync)) {
            doVsync = !doVsync;
        }

        if (ImGui::RadioButton("Animate Light", moveLight)) {
            moveLight = !moveLight;
        }

        if (ImGui::TreeNode("Screen Texture")) {
            if (ImGui::Selectable(nameof(finalTexture), activeScreenTexture->ImGuiID() == finalTexture.ImGuiID()))
                activeScreenTexture = &finalTexture;
            if (ImGui::Selectable(nameof(hdrTexture), activeScreenTexture->ImGuiID() == hdrTexture.ImGuiID()))
                activeScreenTexture = &hdrTexture;
            if (ImGui::Selectable(nameof(albedoTexture), activeScreenTexture->ImGuiID() == albedoTexture.ImGuiID()))
                activeScreenTexture = &albedoTexture;
            if (ImGui::Selectable(nameof(normalTexture), activeScreenTexture->ImGuiID() == normalTexture.ImGuiID()))
                activeScreenTexture = &normalTexture;
            if (ImGui::Selectable(nameof(positionTexture), activeScreenTexture->ImGuiID() == positionTexture.ImGuiID()))
                activeScreenTexture = &positionTexture;
            if (ImGui::Selectable(nameof(shadowTexture), activeScreenTexture->ImGuiID() == shadowTexture.ImGuiID()))
                activeScreenTexture = &shadowTexture;
            if (ImGui::Selectable(nameof(preprocessTexture), activeScreenTexture->ImGuiID() == preprocessTexture.ImGuiID()))
                activeScreenTexture = &preprocessTexture;
            if (ImGui::Selectable(nameof(bloomTexture), activeScreenTexture->ImGuiID() == bloomTexture.ImGuiID()))
                activeScreenTexture = &bloomTexture;
            if (ImGui::Selectable(nameof(ssaoColorTexture), activeScreenTexture->ImGuiID() == ssaoColorTexture.ImGuiID()))
                activeScreenTexture = &ssaoColorTexture;
            if (ImGui::Selectable(nameof(ssaoBlurTexture), activeScreenTexture->ImGuiID() == ssaoBlurTexture.ImGuiID()))
                activeScreenTexture = &ssaoBlurTexture;
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
        if (ImGui::DragFloat2("Angle", glm::value_ptr(sunCamera.getAngle()), 0.01f)) {}
        
        if (ImGui::DragFloat2("Planes", glm::value_ptr(planes), 0.1f)) {
            sunCamera.getProjection() = glm::orthoRH_ZO(-orthoSize, orthoSize, -orthoSize, orthoSize, planes.x, planes.y);
        }
        if (ImGui::DragFloat("Size", &orthoSize)) {
            sunCamera.getProjection() = glm::orthoRH_ZO(-orthoSize, orthoSize, -orthoSize, orthoSize, planes.x, planes.y);
        }
        if (ImGui::DragFloat("Min bias", &uniforms.minBias, 0.0001f, 0.0f, FLT_MAX, "%.4f")) {}
        if (ImGui::DragFloat("Max bias", &uniforms.maxBias, 0.0001f, 0.0f, FLT_MAX, "%.4f")) {}
        if (ImGui::ColorEdit3("Color", glm::value_ptr(uniforms.sunColor))) {}
        
        ImGui::NewLine();
        ImGui::Text("Point Light");
        ImGui::Separator();
        if (ImGui::DragFloat("far plane", &farPlane)) {
            shadowProj = glm::perspective(glm::radians(90.0f), float(SHADOW_WIDTH / SHADOW_HEIGHT), nearPlane, farPlane);
        }
        
        ImGui::NewLine();
        ImGui::Text("Normal Maps");
        ImGui::Separator();

        static bool doNormalMapping = false;
        if (ImGui::Checkbox("Enable ##normalmapping", &doNormalMapping)) {
            if (!doNormalMapping) {
                gbufferStages[0].defines = { "NO_NORMAL_MAP" };
                gbufferStages[1].defines = { "NO_NORMAL_MAP" };
            }
            else {
                gbufferStages[0].defines.clear();
                gbufferStages[1].defines.clear();
            }
            gbufferShader->reload(gbufferStages.data(), gbufferStages.size());
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
            scene.camera.getProjection() = glm::perspectiveRH(glm::radians(fov), (float)renderWidth / (float)renderHeight, 0.1f, 100.0f);
            ImGuizmo::SetRect(pos.x, pos.y, size.x, size.y);

            // resizing framebuffers
            albedoTexture.bind();
            albedoTexture.init(renderWidth, renderHeight, Format::RGBA_F16);

            normalTexture.bind();
            normalTexture.init(renderWidth, renderHeight, Format::RGBA_F16);

            positionTexture.bind();
            positionTexture.init(renderWidth, renderHeight, Format::RGBA_F16);

            GDepthBuffer.init(renderWidth, renderHeight, GL_DEPTH32F_STENCIL8);

            preprocessTexture.bind();
            preprocessTexture.init(renderWidth, renderHeight, Format::RGBA_F16);

            hdrTexture.bind();
            hdrTexture.init(renderWidth, renderHeight, Format::RGBA_F16);

            hdrRenderbuffer.init(renderWidth, renderHeight, GL_DEPTH32F_STENCIL8);

            finalTexture.bind();
            finalTexture.init(renderWidth, renderHeight, Format::RGB_F);

            ssaoColorTexture.bind();
            ssaoColorTexture.init(renderWidth, renderHeight, Format::RGB_F);

            finalRenderbuffer.init(renderWidth, renderHeight, GL_DEPTH32F_STENCIL8);

            bloomTexture.bind();
            bloomTexture.init(renderWidth, renderHeight, Format::RGBA_F16);

            for (unsigned int i = 0; i < 2; i++) {
                pingpongTextures[i].bind();
                pingpongTextures[i].init(renderWidth, renderHeight, Format::RGBA_F16);
            }

            ssaoBlurTexture.bind();
            ssaoBlurTexture.init(renderWidth, renderHeight, Format::RGB_F);

            resizing = false;
        }

        deltaTimer.stop();
        deltaTime = deltaTimer.elapsedMs();

    } // while true loop

    display = SDL_GetWindowDisplayIndex(directxwindow);
    //clean up SDL
    SDL_DestroyWindow(directxwindow);
    SDL_Quit();
} // main

} // namespace Raekor  