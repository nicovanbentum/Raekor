#include "pch.h"
#include "renderpass.h"
#include "ecs.h"
#include "camera.h"

namespace Raekor {
namespace RenderPass {

ShadowMap::ShadowMap(uint32_t width, uint32_t height) {
    // load shaders from disk
    std::vector<Shader::Stage> shadowmapStages;
    shadowmapStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\depth.vert");
    shadowmapStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\depth.frag");
    shader.reload(shadowmapStages.data(), shadowmapStages.size());

    // init uniform buffer
    uniformBuffer.setSize(sizeof(uniforms));

    // init render target
    result.bind();
    result.init(width, height, Format::DEPTH);
    result.setFilter(Sampling::Filter::None);
    result.setWrap(Sampling::Wrap::ClampBorder);

    framebuffer.bind();
    framebuffer.attach(result, GL_DEPTH_ATTACHMENT);
    framebuffer.unbind();
}

void ShadowMap::execute(Scene& scene, Camera& sunCamera) {
    // setup the shadow map 
    framebuffer.bind();
    glClear(GL_DEPTH_BUFFER_BIT);
    glCullFace(GL_FRONT);

    // render the entire scene to the directional light shadow map
    shader.bind();
    uniforms.cameraMatrix = sunCamera.getProjection() * sunCamera.getView();
    uniformBuffer.update(&uniforms, sizeof(uniforms));
    uniformBuffer.bind(0);

    for (auto& object : scene) {
        shader.getUniform("model") = object.transform;
        object.render();
    }
}

//////////////////////////////////////////////////////////////////////////////////

OmniShadowMap::OmniShadowMap(uint32_t width, uint32_t height) {
    settings.width = width, settings.height = height;

    std::vector<Shader::Stage> omniShadowmapStages;
    omniShadowmapStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\depthCube.vert");
    omniShadowmapStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\depthCube.frag");
    shader.reload(omniShadowmapStages.data(), omniShadowmapStages.size());

    result.bind();
    for (unsigned int i = 0; i < 6; ++i) {
        result.init(settings.width, settings.height, i, Format::DEPTH, nullptr);
    }
    result.setFilter(Sampling::Filter::None);
    result.setWrap(Sampling::Wrap::ClampEdge);

    depthCubeFramebuffer.bind();
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    depthCubeFramebuffer.unbind();
}

void OmniShadowMap::execute(Scene& scene, const glm::vec3& lightPosition) {
    // generate the view matrices for calculating lightspace
    std::vector<glm::mat4> shadowTransforms;
    glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), float(settings.width / settings.height), settings.nearPlane, settings.farPlane);

    shadowTransforms.reserve(6);
    shadowTransforms.push_back(shadowProj * glm::lookAtRH(lightPosition, lightPosition + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
    shadowTransforms.push_back(shadowProj * glm::lookAtRH(lightPosition, lightPosition + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
    shadowTransforms.push_back(shadowProj * glm::lookAtRH(lightPosition, lightPosition + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)));
    shadowTransforms.push_back(shadowProj * glm::lookAtRH(lightPosition, lightPosition + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0)));
    shadowTransforms.push_back(shadowProj * glm::lookAtRH(lightPosition, lightPosition + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0)));
    shadowTransforms.push_back(shadowProj * glm::lookAtRH(lightPosition, lightPosition + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0)));

    // render every model using the depth cubemap shader
    depthCubeFramebuffer.bind();
    glCullFace(GL_BACK);

    shader.bind();
    shader.getUniform("farPlane") = settings.farPlane;
    for (uint32_t i = 0; i < 6; i++) {
        depthCubeFramebuffer.attach(result, GL_DEPTH_ATTACHMENT, i);
        glClear(GL_DEPTH_BUFFER_BIT);
        shader.getUniform("projView") = shadowTransforms[i];
        shader.getUniform("lightPos") = lightPosition;

        for (auto& object : scene) {
            shader.getUniform("model") = object.transform;
            object.render();
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////

GeometryBuffer::GeometryBuffer(Viewport& viewport) {
    std::vector<Shader::Stage> gbufferStages;
    gbufferStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\gbuffer.vert");
    gbufferStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\gbuffer.frag");
    gbufferStages[0].defines = { "NO_NORMAL_MAP" };
    gbufferStages[1].defines = { "NO_NORMAL_MAP" };
    shader.reload(gbufferStages.data(), gbufferStages.size());

    albedoTexture.bind();
    albedoTexture.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);
    albedoTexture.setFilter(Sampling::Filter::None);
    albedoTexture.unbind();

    normalTexture.bind();
    normalTexture.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);
    normalTexture.setFilter(Sampling::Filter::None);
    normalTexture.unbind();

    positionTexture.bind();
    positionTexture.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);
    positionTexture.setFilter(Sampling::Filter::None);
    positionTexture.setWrap(Sampling::Wrap::ClampEdge);
    positionTexture.unbind();

    GDepthBuffer.init(viewport.size.x, viewport.size.y, GL_DEPTH32F_STENCIL8);

    GBuffer.bind();
    GBuffer.attach(positionTexture, GL_COLOR_ATTACHMENT0);
    GBuffer.attach(normalTexture, GL_COLOR_ATTACHMENT1);
    GBuffer.attach(albedoTexture, GL_COLOR_ATTACHMENT2);
    GBuffer.attach(GDepthBuffer, GL_DEPTH_STENCIL_ATTACHMENT);
    GBuffer.unbind();
}

void GeometryBuffer::execute(Scene& scene, Viewport& viewport) {
    GBuffer.bind();

    shader.bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader.getUniform("projection") = viewport.getCamera().getProjection();
    shader.getUniform("view") = viewport.getCamera().getView();

    for (auto& object : scene) {
        shader.getUniform("model") = object.transform;
        object.render();
    }

    GBuffer.unbind();
}

void GeometryBuffer::resize(Viewport& viewport) {
    // resizing framebuffers
    albedoTexture.bind();
    albedoTexture.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);

    normalTexture.bind();
    normalTexture.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);

    positionTexture.bind();
    positionTexture.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);

    GDepthBuffer.init(viewport.size.x, viewport.size.y, GL_DEPTH32F_STENCIL8);
}

//////////////////////////////////////////////////////////////////////////////////

ScreenSpaceAmbientOcclusion::ScreenSpaceAmbientOcclusion(Viewport& viewport) {
    noiseScale = { viewport.size.x / 4.0f, viewport.size.y / 4.0f };

    // load shaders from disk
    std::vector<Shader::Stage> ssaoStages;
    ssaoStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\SSAO.vert");
    ssaoStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\SSAO.frag");
    shader.reload(ssaoStages.data(), ssaoStages.size());

    std::vector<Shader::Stage> ssaoBlurStages;
    ssaoBlurStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\quad.vert");
    ssaoBlurStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\SSAOblur.frag");
    blurShader.reload(ssaoBlurStages.data(), ssaoBlurStages.size());


    // create SSAO kernel hemisphere
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
    std::default_random_engine generator;
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

    // create data for the random noise texture
    std::vector<glm::vec3> ssaoNoise;
    for (unsigned int i = 0; i < 16; i++) {
        glm::vec3 noise(
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) * 2.0 - 1.0,
            0.0f);
        ssaoNoise.push_back(noise);
    }

    // init textures and framebuffers
    noise.bind();
    noise.init(4, 4, { GL_RGB16F, GL_RGB, GL_FLOAT }, &ssaoNoise[0]);
    noise.setFilter(Sampling::Filter::None);
    noise.setWrap(Sampling::Wrap::Repeat);

    preblurResult.bind();
    preblurResult.init(viewport.size.x, viewport.size.y, { GL_RGBA, GL_RGBA, GL_FLOAT }, nullptr);
    preblurResult.setFilter(Sampling::Filter::None);

    framebuffer.bind();
    framebuffer.attach(preblurResult, GL_COLOR_ATTACHMENT0);

    result.bind();
    result.init(viewport.size.x, viewport.size.y, { GL_RGBA, GL_RGBA, GL_FLOAT }, nullptr);
    result.setFilter(Sampling::Filter::None);

    blurFramebuffer.bind();
    blurFramebuffer.attach(result, GL_COLOR_ATTACHMENT0);
}

void ScreenSpaceAmbientOcclusion::execute(Scene& scene, Viewport& viewport, GeometryBuffer* geometryPass, Mesh* quad) {
    framebuffer.bind();
    geometryPass->positionTexture.bindToSlot(0);
    geometryPass->normalTexture.bindToSlot(1);
    noise.bindToSlot(2);
    shader.bind();

    shader.getUniform("samples") = ssaoKernel;
    shader.getUniform("view") = viewport.getCamera().getView();
    shader.getUniform("projection") = viewport.getCamera().getProjection();
    shader.getUniform("noiseScale") = noiseScale;
    shader.getUniform("sampleCount") = settings.samples;
    shader.getUniform("power") = settings.power;
    shader.getUniform("bias") = settings.bias;

    quad->render();

    // now blur the SSAO result
    blurFramebuffer.bind();
    preblurResult.bindToSlot(0);
    blurShader.bind();

    quad->render();
}

void ScreenSpaceAmbientOcclusion::resize(Viewport& viewport) {
    noiseScale = { viewport.size.x / 4.0f, viewport.size.y / 4.0f };

    preblurResult.bind();
    preblurResult.init(viewport.size.x, viewport.size.y, Format::RGB_F);

    result.bind();
    result.init(viewport.size.x, viewport.size.y, Format::RGB_F);
}

//////////////////////////////////////////////////////////////////////////////////

DeferredLighting::DeferredLighting(Viewport& viewport) {
    // load shaders from disk
    Shader::Stage vertex(Shader::Type::VERTEX, "shaders\\OpenGL\\main.vert");
    Shader::Stage frag(Shader::Type::FRAG, "shaders\\OpenGL\\main.frag");
    std::array<Shader::Stage, 2> modelStages = { vertex, frag };
    shader.reload(modelStages.data(), modelStages.size());

    // init render targets
    result.bind();
    result.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);
    result.setFilter(Sampling::Filter::Bilinear);
    result.unbind();

    bloomHighlights.bind();
    bloomHighlights.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);
    bloomHighlights.setFilter(Sampling::Filter::Bilinear);
    bloomHighlights.unbind();

    renderbuffer.init(viewport.size.x, viewport.size.y, GL_DEPTH32F_STENCIL8);

    framebuffer.bind();
    framebuffer.attach(result, GL_COLOR_ATTACHMENT0);
    framebuffer.attach(bloomHighlights, GL_COLOR_ATTACHMENT1);
    framebuffer.attach(renderbuffer, GL_DEPTH_STENCIL_ATTACHMENT);
    framebuffer.unbind();

    // init uniform buffer
    uniformBuffer.setSize(sizeof(uniforms));
}

void DeferredLighting::execute(Scene& scene, Viewport& viewport, ShadowMap* shadowMap, OmniShadowMap* omniShadowMap, 
                                GeometryBuffer* GBuffer, ScreenSpaceAmbientOcclusion* ambientOcclusion, Mesh* quad) {
    // bind the main framebuffer
    framebuffer.bind();
    Renderer::Clear({ 0.0f, 0.0f, 0.0f, 1.0f });

    // set uniforms
    shader.bind();
    shader.getUniform("sunColor") = settings.sunColor;
    shader.getUniform("minBias") = settings.minBias;
    shader.getUniform("maxBias") = settings.maxBias;
    shader.getUniform("farPlane") = settings.farPlane;
    shader.getUniform("bloomThreshold") = settings.bloomThreshold;

    // bind textures to shader binding slots
    shadowMap->result.bindToSlot(0);
    omniShadowMap->result.bindToSlot(1);
    GBuffer->positionTexture.bindToSlot(2);
    GBuffer->albedoTexture.bindToSlot(3);
    GBuffer->normalTexture.bindToSlot(4);
    ambientOcclusion->result.bindToSlot(5);

    // update the uniform buffer CPU side
    uniforms.view = viewport.getCamera().getView();
    uniforms.projection = viewport.getCamera().getProjection();
    uniforms.pointLightPos = glm::vec4(scene.pointLight.position, 1.0f);
    uniforms.DirLightPos = glm::vec4(scene.sunCamera.getPosition(), 1.0);
    uniforms.DirViewPos = glm::vec4(viewport.getCamera().getPosition(), 1.0);
    uniforms.lightSpaceMatrix = scene.sunCamera.getProjection() * scene.sunCamera.getView();

    // update uniform buffer GPU side
    uniformBuffer.update(&uniforms, sizeof(uniforms));
    uniformBuffer.bind(0);

    // perform lighting pass and unbind
    quad->render();
    framebuffer.unbind();
}

void DeferredLighting::resize(Viewport& viewport) {
    // resize render targets
    result.bind();
    result.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);

    bloomHighlights.bind();
    bloomHighlights.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);

    renderbuffer.init(viewport.size.x, viewport.size.y, GL_DEPTH32F_STENCIL8);
}


//////////////////////////////////////////////////////////////////////////////////

Bloom::Bloom(Viewport& viewport) {
    // load shaders from disk
    std::vector<Shader::Stage> bloomStages;
    bloomStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\quad.vert");
    bloomStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\bloom.frag");
    bloomShader.reload(bloomStages.data(), bloomStages.size());

    std::vector<Shader::Stage> blurStages;
    blurStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\quad.vert");
    blurStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\gaussian.frag");
    blurShader.reload(blurStages.data(), blurStages.size());

    // init render targets
    result.bind();
    result.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);
    result.setFilter(Sampling::Filter::Bilinear);
    result.unbind();

    resultFramebuffer.bind();
    resultFramebuffer.attach(result, GL_COLOR_ATTACHMENT0);
    resultFramebuffer.unbind();

    for (unsigned int i = 0; i < 2; i++) {
        blurTextures[i].bind();
        blurTextures[i].init(viewport.size.x, viewport.size.y, Format::RGBA_F16);
        blurTextures[i].setFilter(Sampling::Filter::Bilinear);
        blurTextures[i].setWrap(Sampling::Wrap::ClampEdge);
        blurTextures[i].unbind();

        blurBuffers[i].bind();
        blurBuffers[i].attach(blurTextures[i], GL_COLOR_ATTACHMENT0);
        blurBuffers[i].unbind();
    }
}

void Bloom::execute(glTexture2D& scene, glTexture2D& highlights, Mesh* quad) {
    // perform gaussian blur on the bloom texture using "ping pong" framebuffers that
    // take each others color attachments as input and perform a directional gaussian blur each
    // iteration
    bool horizontal = true, firstIteration = true;
    blurShader.bind();
    for (unsigned int i = 0; i < 10; i++) {
        blurBuffers[horizontal].bind();
        blurShader.getUniform("horizontal") = horizontal;
        if (firstIteration) {
            highlights.bindToSlot(0);
            firstIteration = false;
        }
        else {
            blurTextures[!horizontal].bindToSlot(0);
        }
        quad->render();
        horizontal = !horizontal;
    }

    blurShader.unbind();

    // blend the bloom and scene texture together
    resultFramebuffer.bind();
    bloomShader.bind();
    scene.bindToSlot(0);
    blurTextures[!horizontal].bindToSlot(1);
    quad->render();
    resultFramebuffer.unbind();
}

void Bloom::resize(Viewport& viewport) {
    result.bind();
    result.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);

    for (unsigned int i = 0; i < 2; i++) {
        blurTextures[i].bind();
        blurTextures[i].init(viewport.size.x, viewport.size.y, Format::RGBA_F16);
    }
}

//////////////////////////////////////////////////////////////////////////////////

Tonemapping::Tonemapping(Viewport& viewport) {
    // load shaders from disk
    std::vector<Shader::Stage> tonemapStages;
    tonemapStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\HDR.vert");
    tonemapStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\HDR.frag");
    shader.reload(tonemapStages.data(), tonemapStages.size());

    // init render targets
    result.bind();
    result.init(viewport.size.x, viewport.size.y, Format::RGB_F);
    result.setFilter(Sampling::Filter::None);
    result.unbind();

    renderbuffer.init(viewport.size.x, viewport.size.y, GL_DEPTH32F_STENCIL8);

    framebuffer.bind();
    framebuffer.attach(result, GL_COLOR_ATTACHMENT0);
    framebuffer.attach(renderbuffer, GL_DEPTH_STENCIL_ATTACHMENT);
    framebuffer.unbind();

    // init uniform buffer
    uniformBuffer.setSize(sizeof(settings));
}

void Tonemapping::resize(Viewport& viewport) {
    // resize render targets
    result.bind();
    result.init(viewport.size.x, viewport.size.y, Format::RGB_F);

    renderbuffer.init(viewport.size.x, viewport.size.y, GL_DEPTH32F_STENCIL8);
}

void Tonemapping::execute(glTexture2D& scene, Mesh* quad) {
    // bind and clear render target
    framebuffer.bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // bind shader and input texture
    shader.bind();
    scene.bindToSlot(0);

    // update uniform buffer GPU side
    uniformBuffer.update(&settings, sizeof(settings));
    uniformBuffer.bind(0);

    // render fullscreen quad to perform tonemapping
    quad->render();
    framebuffer.unbind();
}

//////////////////////////////////////////////////////////////////////////////////

Voxelization::Voxelization(uint32_t width, uint32_t height, uint32_t depth) {
    // load shaders from disk
    std::vector<Shader::Stage> voxelStages;
    voxelStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\voxelize.vert");
    voxelStages.emplace_back(Shader::Type::GEO, "shaders\\OpenGL\\voxelize.geom");
    voxelStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\voxelize.frag");
    shader.reload(voxelStages.data(), voxelStages.size());

    // Generate texture on GPU.
    glGenTextures(1, &result);
    glBindTexture(GL_TEXTURE_3D, result);

    // Parameter options.
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Upload texture buffer.
    glTexStorage3D(GL_TEXTURE_3D, 7, GL_RGBA32F, width, height, depth);
    GLfloat clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    glClearTexImage(result, 0, GL_RGBA, GL_FLOAT, &clearColor);
    glGenerateMipmap(GL_TEXTURE_3D);
    glBindTexture(GL_TEXTURE_3D, 0);
}


void Voxelization::execute(Scene& scene, Viewport& viewport) {
    shader.bind();

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glBindImageTexture(1, result, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    shader.getUniform("view") = viewport.getCamera().getView();
    shader.getUniform("projection") = viewport.getCamera().getProjection();

    for (auto& object : scene) {
        shader.getUniform("model") = object.transform;
        object.render();
    }

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
}

//////////////////////////////////////////////////////////////////////////////////

VoxelizationDebug::VoxelizationDebug(Viewport& viewport) {
    std::vector<Shader::Stage> basicStages;
    basicStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\basic.vert");
    basicStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\basic.frag");
    basicShader.reload(basicStages.data(), basicStages.size());


    std::vector<Shader::Stage> voxelDebugStages;
    voxelDebugStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\voxelDebug.vert");
    voxelDebugStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\voxelDebug.frag");
    voxelTracedShader.reload(voxelDebugStages.data(), voxelDebugStages.size());

    cubeBack.bind();
    cubeBack.init(viewport.size.x, viewport.size.y, Format::RGBA_F);
    cubeBack.setFilter(Sampling::Filter::None);
    cubeBack.unbind();

    cubeFront.bind();
    cubeFront.init(viewport.size.x, viewport.size.y, Format::RGBA_F);
    cubeFront.setFilter(Sampling::Filter::None);
    cubeFront.unbind();

    cubeTexture.init(viewport.size.x, viewport.size.y, GL_DEPTH32F_STENCIL8);

    cubeBackfaceFramebuffer.bind();
    cubeBackfaceFramebuffer.attach(cubeBack, GL_COLOR_ATTACHMENT0);
    cubeBackfaceFramebuffer.unbind();

    cubeFrontfaceFramebuffer.bind();
    cubeFrontfaceFramebuffer.attach(cubeFront, GL_COLOR_ATTACHMENT0);
    cubeFrontfaceFramebuffer.unbind();

    result.bind();
    result.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);
    result.setFilter(Sampling::Filter::None);
    result.unbind();

    voxelVisFramebuffer.bind();
    voxelVisFramebuffer.attach(result, GL_COLOR_ATTACHMENT0);
    voxelVisFramebuffer.unbind();
}

void VoxelizationDebug::execute(Scene& scene, Viewport& viewport, uint32_t voxelMap, Mesh* cube, Mesh* quad) {
    // bind and clear render target
    cubeBackfaceFramebuffer.bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // bind basic to-world shader
    basicShader.bind();
    basicShader.getUniform("projection") = viewport.getCamera().getProjection();
    basicShader.getUniform("view") = viewport.getCamera().getView();
    basicShader.getUniform("model") = glm::mat4(1.0f);

    // render back face culled cube
    cube->render();

    // switch to front culling
    glCullFace(GL_FRONT);

    // bind and clear render target
    cubeFrontfaceFramebuffer.bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // render front face culled cube
    cube->render();

    // disable culling altogether
    glDisable(GL_CULL_FACE);

    // bind cone trace shader and render target
    voxelVisFramebuffer.bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // bind cone trace shader, cube face textures and the voxel map
    voxelTracedShader.bind();
    cubeBack.bindToSlot(0);
    cubeFront.bindToSlot(1);
    glBindTextureUnit(2, voxelMap);

    // upload the camera position for trace direction
    voxelTracedShader.getUniform("cameraPosition") = viewport.getCamera().getPosition();

    // render fullscreen quad to perform rendering
    quad->render();

    // re-enable backface culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // unbind framebuffers
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void VoxelizationDebug::resize(Viewport& viewport) {
    result.bind();
    result.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);

    cubeBack.bind();
    cubeBack.init(viewport.size.x, viewport.size.y, Format::RGBA_F);

    cubeFront.bind();
    cubeFront.init(viewport.size.x, viewport.size.y, Format::RGBA_F);

    cubeTexture.init(viewport.size.x, viewport.size.y, GL_DEPTH32F_STENCIL8);
}

} // renderpass
} // namespace Raekor