#include "pch.h"
#include "renderpass.h"
#include "ecs.h"
#include "camera.h"

namespace Raekor {
namespace RenderPass {

ShadowMap::ShadowMap(uint32_t width, uint32_t height) :
    sunCamera(glm::vec3(0, 0, 0), glm::orthoRH_ZO(
        -settings.size, settings.size, -settings.size, settings.size, 
        settings.planes.x, settings.planes.y
    )) 
{
    sunCamera.getView() = glm::lookAtRH(
        glm::vec3(-2.0f, 12.0f, 2.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    sunCamera.getAngle().y = -1.325f;

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
    result.setFilter(Sampling::Filter::Bilinear);
    result.setWrap(Sampling::Wrap::ClampEdge);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);

    framebuffer.attach(result, GL_DEPTH_ATTACHMENT);
}

void ShadowMap::execute(entt::registry& scene) {
    // setup the shadow map 
    framebuffer.bind();
    glClear(GL_DEPTH_BUFFER_BIT);
    glCullFace(GL_FRONT);

    // render the entire scene to the directional light shadow map
    shader.bind();
    uniforms.cameraMatrix = sunCamera.getProjection() * sunCamera.getView();
    uniformBuffer.update(&uniforms, sizeof(uniforms));
    uniformBuffer.bind(0);

    auto view = scene.view<ECS::MeshComponent, ECS::TransformComponent>();

    for (auto entity : view) {
        auto& mesh = view.get<ECS::MeshComponent>(entity);
        auto& transform = view.get<ECS::TransformComponent>(entity);

        shader.getUniform("model") = transform.worldTransform;

        // determine if we use the original mesh vertices or GPU skinned vertices
        if (scene.has<ECS::MeshAnimationComponent>(entity)) {
            scene.get<ECS::MeshAnimationComponent>(entity).skinnedVertexBuffer.bind();
        }
        else {
            mesh.vertexBuffer.bind();
        }
        mesh.indexBuffer.bind();
        Renderer::DrawIndexed(mesh.indexBuffer.count);
    }

    glCullFace(GL_BACK);
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
}

void OmniShadowMap::execute(entt::registry& scene, const glm::vec3& lightPosition) {
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

        auto view = scene.view<ECS::MeshComponent, ECS::TransformComponent>();

        for (auto entity : view) {
            auto& mesh = view.get<ECS::MeshComponent>(entity);
            auto& transform = view.get<ECS::TransformComponent>(entity);

            shader.getUniform("model") = transform.worldTransform;

            // determine if we use the original mesh vertices or GPU skinned vertices
            if (scene.has<ECS::MeshAnimationComponent>(entity)) {
                scene.get<ECS::MeshAnimationComponent>(entity).skinnedVertexBuffer.bind();
            }
            else {
                mesh.vertexBuffer.bind();
            }
            mesh.indexBuffer.bind();
            Renderer::DrawIndexed(mesh.indexBuffer.count);
        }
    }

    depthCubeFramebuffer.unbind();
}

//////////////////////////////////////////////////////////////////////////////////

GeometryBuffer::GeometryBuffer(Viewport& viewport) {
    std::vector<Shader::Stage> gbufferStages;
    gbufferStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\gbuffer.vert");
    gbufferStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\gbuffer.frag");
    gbufferStages[0].defines = { "NO_NORMAL_MAP" };
    gbufferStages[1].defines = { "NO_NORMAL_MAP" };
    shader.reload(gbufferStages.data(), gbufferStages.size());
    hotloader.watch(&shader, gbufferStages.data(), gbufferStages.size());

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

    materialTexture.bind();
    materialTexture.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);
    materialTexture.setFilter(Sampling::Filter::None);
    materialTexture.unbind();

    GDepthBuffer.init(viewport.size.x, viewport.size.y, GL_DEPTH_COMPONENT32F);

    GBuffer.attach(positionTexture, GL_COLOR_ATTACHMENT0);
    GBuffer.attach(normalTexture, GL_COLOR_ATTACHMENT1);
    GBuffer.attach(albedoTexture, GL_COLOR_ATTACHMENT2);
    GBuffer.attach(materialTexture, GL_COLOR_ATTACHMENT3);

    GBuffer.attach(GDepthBuffer, GL_DEPTH_ATTACHMENT);
}

void GeometryBuffer::execute(entt::registry& scene, Viewport& viewport) {
    hotloader.checkForUpdates();

    GBuffer.bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader.bind();
    shader.getUniform("projection") = viewport.getCamera().getProjection();
    shader.getUniform("view") = viewport.getCamera().getView();

    Math::Frustrum frustrum;
    frustrum.update(viewport.getCamera().getProjection() * viewport.getCamera().getView(), true);

    culled = 0;

    auto view = scene.view<ECS::MeshComponent, ECS::TransformComponent>();

    for (auto entity : view) {
        auto& mesh = view.get<ECS::MeshComponent>(entity);
        auto& transform = view.get<ECS::TransformComponent>(entity);

            // convert AABB from local to world space
            std::array<glm::vec3, 2> worldAABB = {
                transform.worldTransform * glm::vec4(mesh.aabb[0], 1.0),
                transform.worldTransform * glm::vec4(mesh.aabb[1], 1.0)
            };

            // if the frustrum can't see the mesh's OBB we cull it
            if (!frustrum.vsAABB(worldAABB[0], worldAABB[1])) {
                culled += 1;
                continue;
            }


        auto material = scene.try_get<ECS::MaterialComponent>(entity);

        if (material) {
            if (material->albedo)       material->albedo->bindToSlot(0);
            if (material->normals)      material->normals->bindToSlot(3);
            if (material->metalrough)   material->metalrough->bindToSlot(4);
        }

        shader.getUniform("model") = transform.worldTransform;

        shader.getUniform("entity") = static_cast<uint32_t>(entity);

        // determine if we use the original mesh vertices or GPU skinned vertices
        if (scene.has<ECS::MeshAnimationComponent>(entity)) {
            scene.get<ECS::MeshAnimationComponent>(entity).skinnedVertexBuffer.bind();
        } else {
            mesh.vertexBuffer.bind();
        }

        mesh.indexBuffer.bind();
        Renderer::DrawIndexed(mesh.indexBuffer.count);
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

    materialTexture.bind();
    materialTexture.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);

    GDepthBuffer.init(viewport.size.x, viewport.size.y, GL_DEPTH32F_STENCIL8);
}

entt::entity GeometryBuffer::pick(uint32_t x, uint32_t y) {
    glm::vec4 readPixel;
    glGetTextureSubImage(materialTexture.mID, 0, x, y, 0, 1, 1, 1, GL_RGBA, GL_FLOAT, sizeof(glm::vec4), glm::value_ptr(readPixel));
    return static_cast<entt::entity>(readPixel.b);
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

    framebuffer.attach(preblurResult, GL_COLOR_ATTACHMENT0);

    result.bind();
    result.init(viewport.size.x, viewport.size.y, { GL_RGBA, GL_RGBA, GL_FLOAT }, nullptr);
    result.setFilter(Sampling::Filter::None);

    blurFramebuffer.attach(result, GL_COLOR_ATTACHMENT0);
}

void ScreenSpaceAmbientOcclusion::execute(Viewport& viewport, GeometryBuffer* geometryPass, Mesh* quad) {
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

    hotloader.watch(&shader, modelStages.data(), modelStages.size());

    // init render targets
    result.bind();
    result.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);
    result.setFilter(Sampling::Filter::Bilinear);
    result.unbind();

    bloomHighlights.bind();
    bloomHighlights.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);
    bloomHighlights.setFilter(Sampling::Filter::Bilinear);
    bloomHighlights.unbind();

    framebuffer.attach(result, GL_COLOR_ATTACHMENT0);
    framebuffer.attach(bloomHighlights, GL_COLOR_ATTACHMENT1);

    // init uniform buffer
    uniformBuffer.setSize(sizeof(uniforms));
}

void DeferredLighting::execute(entt::registry& sscene, Viewport& viewport, ShadowMap* shadowMap, OmniShadowMap* omniShadowMap,
                                GeometryBuffer* GBuffer, ScreenSpaceAmbientOcclusion* ambientOcclusion, Voxelization* voxels, Mesh* quad) {
    hotloader.checkForUpdates();
    
    // bind the main framebuffer
    framebuffer.bind();
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    //Renderer::Clear({ 0.0f, 0.0f, 0.0f, 1.0f });

    // set uniforms
    shader.bind();
    shader.getUniform("sunColor") = settings.sunColor;
    shader.getUniform("minBias") = settings.minBias;
    shader.getUniform("maxBias") = settings.maxBias;
    shader.getUniform("farPlane") = settings.farPlane;
    shader.getUniform("bloomThreshold") = settings.bloomThreshold;

    shader.getUniform("pointLightCount") = (uint32_t)sscene.view<ECS::PointLightComponent>().size();
    shader.getUniform("directionalLightCount") = (uint32_t)sscene.view<ECS::DirectionalLightComponent>().size();

    shader.getUniform("voxelsWorldSize") = voxels->worldSize;

    // bind textures to shader binding slots
    shadowMap->result.bindToSlot(0);
    
    if (omniShadowMap) {
        omniShadowMap->result.bindToSlot(1);
    }

    GBuffer->positionTexture.bindToSlot(2);
    GBuffer->albedoTexture.bindToSlot(3);
    GBuffer->normalTexture.bindToSlot(4);

    if (ambientOcclusion) {
        ambientOcclusion->result.bindToSlot(5);
    }

    voxels->result.bindToSlot(6);
    glBindTextureUnit(7, GBuffer->materialTexture.mID);

    // update the uniform buffer CPU side
    uniforms.view = viewport.getCamera().getView();
    uniforms.projection = viewport.getCamera().getProjection();

    // update every light type
    // TODO: figure out this directional light crap, I only really want to support a single one
    {
        auto view = sscene.view<ECS::DirectionalLightComponent, ECS::TransformComponent>();
        auto entity = view.front();
        if (entity != entt::null) {
            auto& light = view.get<ECS::DirectionalLightComponent>(entity);
            auto& transform = view.get<ECS::TransformComponent>(entity);

            light.buffer.direction = glm::vec4(shadowMap->sunCamera.getDirection(), 1.0);
            uniforms.dirLights[0] = light.buffer;
        }
    }

    auto posView = sscene.view<ECS::PointLightComponent, ECS::TransformComponent>();
    unsigned int posViewCounter = 0;
    for (auto entity : posView) {
        auto& light = posView.get<ECS::PointLightComponent>(entity);
        auto& transform = posView.get<ECS::TransformComponent>(entity);

        posViewCounter++;
        if (posViewCounter >= ARRAYSIZE(uniforms.pointLights)) {
            break;
        }

        light.buffer.position = glm::vec4(transform.position, 1.0f);
        uniforms.pointLights[posViewCounter] = light.buffer;
    }

    uniforms.cameraPosition = glm::vec4(viewport.getCamera().getPosition(), 1.0);
    uniforms.lightSpaceMatrix = shadowMap->sunCamera.getProjection() * shadowMap->sunCamera.getView();

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

    resultFramebuffer.attach(result, GL_COLOR_ATTACHMENT0);

    for (unsigned int i = 0; i < 2; i++) {
        blurTextures[i].bind();
        blurTextures[i].init(viewport.size.x, viewport.size.y, Format::RGBA_F16);
        blurTextures[i].setFilter(Sampling::Filter::Bilinear);
        blurTextures[i].setWrap(Sampling::Wrap::ClampEdge);
        blurTextures[i].unbind();

        blurBuffers[i].attach(blurTextures[i], GL_COLOR_ATTACHMENT0);
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

    framebuffer.attach(result, GL_COLOR_ATTACHMENT0);

    // init uniform buffer
    uniformBuffer.setSize(sizeof(settings));
}

void Tonemapping::resize(Viewport& viewport) {
    // resize render targets
    result.bind();
    result.init(viewport.size.x, viewport.size.y, Format::RGB_F);
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

Voxelization::Voxelization(int size) : size(size) {
    // load shaders from disk
    std::vector<Shader::Stage> voxelStages;
    voxelStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\voxelize.vert");
    voxelStages.emplace_back(Shader::Type::GEO, "shaders\\OpenGL\\voxelize.geom");
    voxelStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\voxelize.frag");
    shader.reload(voxelStages.data(), voxelStages.size());

    hotloader.watch(&shader, voxelStages.data(), voxelStages.size());

    auto mipStage = Shader::Stage(Shader::Type::COMPUTE, "shaders\\OpenGL\\mipmap.comp");
    mipmapShader.reload(&mipStage, 1);

    auto opacityFixStage = Shader::Stage(Shader::Type::COMPUTE, "shaders\\OpenGL\\correctAlpha.comp");
    opacityFixShader.reload(&opacityFixStage, 1);

    // Direct State Access (TODO: Experimental, implement everywhere)
    result.setFilter(Sampling::Filter::Trilinear);
    auto levelCount = std::log2(size);
    glTextureStorage3D(result.mID, static_cast<GLsizei>(levelCount), GL_RGBA8, size, size, size);
    result.genMipMaps();
}

void Voxelization::computeMipmaps(glTexture3D& texture) {
    int level = 0, texSize = size;
    while (texSize >= 1.0f) {
        texSize = static_cast<int>(texSize * 0.5f);
        mipmapShader.bind();
        glBindImageTexture(0, texture.mID, level, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA8);
        glBindImageTexture(1, texture.mID, level + 1, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
        // TODO: query local work group size at startup
        glDispatchCompute(static_cast<GLuint>(size / 64), texSize, texSize);
        mipmapShader.unbind();
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        level++;
    }
}

void Voxelization::correctOpacity(glTexture3D& texture) {
    opacityFixShader.bind();
    glBindImageTexture(0, texture.mID, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
    // local work group size is 64
    glDispatchCompute(static_cast<GLuint>(size / 64), size, size);
    opacityFixShader.unbind();
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}


void Voxelization::execute(entt::registry& scene, Viewport& viewport, ShadowMap* shadowmap) {
    hotloader.checkForUpdates();

    // left, right, bottom, top, zNear, zFar
    auto projectionMatrix = glm::ortho(-worldSize * 0.5f, worldSize * 0.5f, -worldSize * 0.5f, worldSize * 0.5f, worldSize * 0.5f, worldSize * 1.5f);
    px = projectionMatrix * glm::lookAt(glm::vec3(worldSize, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    py = projectionMatrix * glm::lookAt(glm::vec3(0, worldSize, 0), glm::vec3(0, 0, 0), glm::vec3(0, 0, -1));
    pz = projectionMatrix * glm::lookAt(glm::vec3(0, 0, worldSize), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

    result.clear({ 0, 0, 0, 0 });

    // set GL state
    glViewport(0, 0, size, size);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // bind shader and 3d voxel map
    shader.bind();
    result.bindToSlot(1, GL_WRITE_ONLY, GL_R32UI);
    shadowmap->result.bindToSlot(2);

    shader.getUniform("lightViewProjection") = shadowmap->sunCamera.getProjection() * shadowmap->sunCamera.getView();


    auto view = scene.view<ECS::MeshComponent, ECS::TransformComponent>();

    for (auto entity : view) {

        auto& mesh = view.get<ECS::MeshComponent>(entity);
        auto& transform = view.get<ECS::TransformComponent>(entity);

        ECS::MaterialComponent* material = scene.try_get<ECS::MaterialComponent>(entity);

        shader.getUniform("model") = transform.worldTransform;
        shader.getUniform("px") = px;
        shader.getUniform("py") = py;
        shader.getUniform("pz") = pz;

        if (material) {
            if (material->albedo) material->albedo->bindToSlot(0);
        }

        // determine if we use the original mesh vertices or GPU skinned vertices
        if (scene.has<ECS::MeshAnimationComponent>(entity)) {
            scene.get<ECS::MeshAnimationComponent>(entity).skinnedVertexBuffer.bind();
        }
        else {
            mesh.vertexBuffer.bind();
        }
        mesh.indexBuffer.bind();
        Renderer::DrawIndexed(mesh.indexBuffer.count);
    }

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // Run compute shaders
    correctOpacity(result);
    computeMipmaps(result);
    //result.genMipMaps();

    // reset OpenGL state
    glViewport(0, 0, viewport.size.x, viewport.size.y);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
}

//////////////////////////////////////////////////////////////////////////////////

VoxelizationDebug::VoxelizationDebug(Viewport& viewport) {
    std::vector<Shader::Stage> voxelDebugStages;
    voxelDebugStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\voxelDebug.vert");
    voxelDebugStages.emplace_back(Shader::Type::GEO, "shaders\\OpenGL\\voxelDebug.geom");
    voxelDebugStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\voxelDebug.frag");
    shader.reload(voxelDebugStages.data(), voxelDebugStages.size());

    renderBuffer.init(viewport.size.x, viewport.size.y, GL_DEPTH32F_STENCIL8);

    frameBuffer.attach(renderBuffer, GL_DEPTH_STENCIL_ATTACHMENT);
}

void VoxelizationDebug::execute(Viewport& viewport, glTexture2D& input, Voxelization* voxels) {
    // bind the input framebuffer, we draw the debug vertices on top
    frameBuffer.bind();
    frameBuffer.attach(input, GL_COLOR_ATTACHMENT0);
    glClear(GL_DEPTH_BUFFER_BIT);

    float voxelSize = voxels->worldSize / 128;
    glm::mat4 modelMatrix = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(voxelSize)), glm::vec3(0, 0, 0));

    // bind shader and set uniforms
    shader.bind();
    shader.getUniform("voxelSize") = voxels->worldSize / 128;
    shader.getUniform("p") = viewport.getCamera().getProjection();
    shader.getUniform("mv") = viewport.getCamera().getView() * modelMatrix;

    voxels->result.bindToSlot(0);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(std::pow(128, 3)));

    // unbind framebuffers
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void VoxelizationDebug::resize(Viewport& viewport) {
    renderBuffer.init(viewport.size.x, viewport.size.y, GL_DEPTH32F_STENCIL8);
}

//////////////////////////////////////////////////////////////////////////////////

BoundingBoxDebug::BoundingBoxDebug(Viewport& viewport) {
    // load shaders from disk
    std::vector<Shader::Stage> aabbStages;
    aabbStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\aabb.vert");
    aabbStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\aabb.frag");
    shader.reload(aabbStages.data(), aabbStages.size());

    result.bind();
    result.init(viewport.size.x, viewport.size.y, Format::RGBA_F);
    result.setFilter(Sampling::Filter::None);
    result.unbind();

    std::vector<uint32_t> indices = {
    0, 1, 1, 2, 2, 3, 3, 0, 4,
    5, 5, 6, 6, 7, 7, 4, 0, 0,
    0, 4, 1, 5, 2, 6, 3, 7, 7
    };
    indexBuffer.loadIndices(indices.data(), indices.size());

    vertexBuffer.setLayout({
    {"POSITION",    ShaderType::FLOAT3},
    {"UV",          ShaderType::FLOAT2},
    {"NORMAL",      ShaderType::FLOAT3},
    {"TANGENT",     ShaderType::FLOAT3},
    {"BINORMAL",    ShaderType::FLOAT3},
        });
}

void BoundingBoxDebug::execute(entt::registry& scene, Viewport& viewport, glTexture2D& texture, glRenderbuffer& renderBuffer, entt::entity active) {
    
    assert(active != entt::null);
    if (!scene.has<ECS::MeshComponent>(active) || !scene.has<ECS::TransformComponent>(active)) {
        return;
    }

    auto& mesh = scene.get<ECS::MeshComponent>(active);
    auto& transform = scene.get<ECS::TransformComponent>(active);

    glEnable(GL_LINE_SMOOTH);

    frameBuffer.bind();
    frameBuffer.attach(texture, GL_COLOR_ATTACHMENT0);
    frameBuffer.attach(renderBuffer, GL_DEPTH_STENCIL_ATTACHMENT);

    shader.bind();
    shader.getUniform("projection") = viewport.getCamera().getProjection();
    shader.getUniform("view") = viewport.getCamera().getView();
    shader.getUniform("model") = transform.worldTransform;

    // calculate obb from aabb
    const auto min = mesh.aabb[0];
    const auto max = mesh.aabb[1];
    
    std::vector<Vertex> vertices = {
        { {min} },
        { {max[0], min[1], min[2] } },

        { {max[0], max[1], min[2] } },
        { {min[0], max[1], min[2] } },

        { {min[0], min[1], max[2] } },
        { {max[0], min[1], max[2] } },

        { {max} },
        { {min[0], max[1], max[2] } },
    };

    vertexBuffer.loadVertices(vertices.data(), vertices.size());

    vertexBuffer.bind();
    indexBuffer.bind();

    glDrawElements(GL_LINES, indexBuffer.count, GL_UNSIGNED_INT, nullptr);

    glDisable(GL_LINE_SMOOTH);

    frameBuffer.unbind();
}

void BoundingBoxDebug::resize(Viewport& viewport) {
    result.bind();
    result.init(viewport.size.x, viewport.size.y, Format::RGBA_F);
    result.unbind();
}

ForwardLightingPass::ForwardLightingPass(Viewport& viewport) {
    // load shaders from disk
    std::vector<Shader::Stage> stages;
    stages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\fwdLight.vert");
    stages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\fwdLight.frag");
    shader.reload(stages.data(), stages.size());

    hotloader.watch(&shader, stages.data(), stages.size());

    // init render targets
    result.bind();
    result.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);
    result.setFilter(Sampling::Filter::Bilinear);
    result.unbind();

    renderbuffer.init(viewport.size.x, viewport.size.y, GL_DEPTH32F_STENCIL8);

    framebuffer.attach(result, GL_COLOR_ATTACHMENT0);
    framebuffer.attach(renderbuffer, GL_DEPTH_STENCIL_ATTACHMENT);

    // init uniform buffer
    uniformBuffer.setSize(sizeof(uniforms));
}

void ForwardLightingPass::execute(Viewport& viewport, entt::registry& scene, Voxelization* voxels, ShadowMap* shadowmap) {
    hotloader.checkForUpdates();

    // enable stencil stuff
    glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glStencilMask(0xFFFF); // Write to stencil buffer
    glStencilFunc(GL_ALWAYS, 0, 0xFFFF);  // Set any stencil to 0

    // update the uniform buffer CPU side
    uniforms.view = viewport.getCamera().getView();
    uniforms.projection = viewport.getCamera().getProjection();

    // update every light type
    auto dirView = scene.view<ECS::DirectionalLightComponent, ECS::TransformComponent>();
    unsigned int dirLightCounter = 0;
    for (auto entity : dirView) {
        auto& light = dirView.get<ECS::DirectionalLightComponent>(entity);
        auto& transform = dirView.get<ECS::TransformComponent>(entity);

        dirLightCounter++;
        if (dirLightCounter >= ARRAYSIZE(uniforms.dirLights)) {
            break;
        }

        light.buffer.direction = glm::vec4(shadowmap->sunCamera.getDirection(), 1.0);
        uniforms.dirLights[dirLightCounter] = light.buffer;
    }

    auto posView = scene.view<ECS::PointLightComponent, ECS::TransformComponent>();
    unsigned int posViewCounter = 0;
    for (auto entity : posView) {
        auto& light = posView.get<ECS::PointLightComponent>(entity);
        auto& transform = posView.get<ECS::TransformComponent>(entity);

        posViewCounter++;
        if (posViewCounter >= ARRAYSIZE(uniforms.pointLights)) {
            break;
        }

        light.buffer.position = glm::vec4(transform.position, 1.0f);
        uniforms.pointLights[posViewCounter] = light.buffer;
    }

    uniforms.cameraPosition = glm::vec4(viewport.getCamera().getPosition(), 1.0);
    uniforms.lightSpaceMatrix = shadowmap->sunCamera.getProjection() * shadowmap->sunCamera.getView();

    // update uniform buffer GPU side
    uniformBuffer.update(&uniforms, sizeof(uniforms));

    framebuffer.bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    shader.bind();
    uniformBuffer.bind(0);

    voxels->result.bindToSlot(0);
    shadowmap->result.bindToSlot(3);

    Math::Frustrum frustrum;
    frustrum.update(viewport.getCamera().getProjection() * viewport.getCamera().getView(), true);
    culled = 0;

    auto view = scene.view<ECS::MeshComponent, ECS::TransformComponent>();

    for (auto entity : view) {
        auto& mesh = scene.get<ECS::MeshComponent>(entity);
        auto& transform = scene.get<ECS::TransformComponent>(entity);

        // convert AABB from local to world space
        std::array<glm::vec3, 2> worldAABB = {
            transform.worldTransform * glm::vec4(mesh.aabb[0], 1.0),
            transform.worldTransform * glm::vec4(mesh.aabb[1], 1.0)
        };

        // if the frustrum can't see the mesh's OBB we cull it
        if (!frustrum.vsAABB(worldAABB[0], worldAABB[1])) {
            culled += 1;
            continue;
        }

        ECS::MaterialComponent* material = scene.try_get<ECS::MaterialComponent>(entity);

        if (material) {
            if (material->albedo) material->albedo->bindToSlot(1);
            if (material->normals) material->normals->bindToSlot(2);
        }

        shader.getUniform("model") = transform.worldTransform;

        // write the entity ID to the stencil buffer for picking
        glStencilFunc(GL_ALWAYS, (GLint)entity, 0xFFFF);

        // determine if we use the original mesh vertices or GPU skinned vertices
        if (scene.has<ECS::MeshAnimationComponent>(entity)) {
            scene.get<ECS::MeshAnimationComponent>(entity).skinnedVertexBuffer.bind();
        }
        else {
            mesh.vertexBuffer.bind();
        }
        mesh.indexBuffer.bind();
        Renderer::DrawIndexed(mesh.indexBuffer.count);
    }

    // disable stencil stuff
    glStencilFunc(GL_ALWAYS, 0, 0xFFFF);  // Set any stencil to 0
    glDisable(GL_STENCIL_TEST);

    framebuffer.unbind();
}

void ForwardLightingPass::resize(Viewport& viewport) {
    result.bind();
    result.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);

    renderbuffer.init(viewport.size.x, viewport.size.y, GL_DEPTH32F_STENCIL8);
}

SkyPass::SkyPass(Viewport& viewport) {
    std::vector<glShader::Stage> stages;
    stages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\sky.vert");
    stages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\sky.frag");
    shader.reload(stages.data(), stages.size());
    hotloader.watch(&shader, stages.data(), stages.size());

    result.bind();
    result.init(viewport.size.x, viewport.size.y, { GL_RGBA32F, GL_RGBA, GL_FLOAT });
    result.setFilter(Sampling::Filter::None);
    result.setWrap(Sampling::Wrap::ClampEdge);
    result.unbind();

    framebuffer.attach(result, GL_COLOR_ATTACHMENT0);
}

void SkyPass::execute(Viewport& viewport, Mesh* quad) {
    hotloader.checkForUpdates();

    framebuffer.bind();
    glClear(GL_COLOR_BUFFER_BIT);

    shader.bind();
    shader.getUniform("projection") = viewport.getCamera().getProjection();
    shader.getUniform("view") = viewport.getCamera().getView();
    shader.getUniform("time") = settings.time;
    shader.getUniform("cirrus") = settings.cirrus;
    shader.getUniform("cumulus") = settings.cumulus;

    quad->render();

    framebuffer.unbind();
}

} // renderpass
} // namespace Raekor