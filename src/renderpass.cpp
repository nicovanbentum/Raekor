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

void ShadowMap::execute(Scene& scene) {
    // setup the shadow map 
    framebuffer.bind();
    glClear(GL_DEPTH_BUFFER_BIT);
    glCullFace(GL_FRONT);

    // render the entire scene to the directional light shadow map
    shader.bind();
    uniforms.cameraMatrix = sunCamera.getProjection() * sunCamera.getView();
    uniformBuffer.update(&uniforms, sizeof(uniforms));
    uniformBuffer.bind(0);

    for (uint32_t i = 0; i < scene.meshes.getCount(); i++) {
        ECS::Entity entity = scene.meshes.getEntity(i);

        ECS::MeshComponent& mesh = scene.meshes[i];
        ECS::TransformComponent* transform = scene.transforms.getComponent(entity);

        if (transform) {
            shader.getUniform("model") = transform->matrix;
        } else {
            shader.getUniform("model") = glm::mat4(1.0f);
        }

        mesh.vertexBuffer.bind();
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

        for (uint32_t i = 0; i < scene.meshes.getCount(); i++) {
            ECS::Entity entity = scene.meshes.getEntity(i);

            ECS::MeshComponent& mesh = scene.meshes[i];
            ECS::TransformComponent* transform = scene.transforms.getComponent(entity);
            const glm::mat4& worldTransform = transform ? transform->matrix : glm::mat4(1.0f);

            shader.getUniform("model") = worldTransform;

            mesh.vertexBuffer.bind();
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

    GBuffer.attach(positionTexture, GL_COLOR_ATTACHMENT0);
    GBuffer.attach(normalTexture, GL_COLOR_ATTACHMENT1);
    GBuffer.attach(albedoTexture, GL_COLOR_ATTACHMENT2);
    GBuffer.attach(GDepthBuffer, GL_DEPTH_STENCIL_ATTACHMENT);
}

void GeometryBuffer::execute(Scene& scene, Viewport& viewport) {
    // enable stencil stuff
    glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glStencilMask(0xFFFF); // Write to stencil buffer
    glStencilFunc(GL_ALWAYS, 0, 0xFFFF);  // Set any stencil to 0

    GBuffer.bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    shader.bind();
    shader.getUniform("projection") = viewport.getCamera().getProjection();
    shader.getUniform("view") = viewport.getCamera().getView();

    Math::Frustrum frustrum;
    frustrum.update(viewport.getCamera().getProjection() * viewport.getCamera().getView(), true);

    culled = 0;

    for (uint32_t i = 0; i < scene.meshes.getCount(); i++) {
        ECS::Entity entity = scene.meshes.getEntity(i);

        ECS::MeshComponent& mesh = scene.meshes[i];

        ECS::NameComponent* name = scene.names.getComponent(entity);

        ECS::TransformComponent* transform = scene.transforms.getComponent(entity);
        const glm::mat4& worldTransform = transform ? transform->matrix : glm::mat4(1.0f);

            // convert AABB from local to world space
            std::array<glm::vec3, 2> worldAABB = {
                worldTransform * glm::vec4(mesh.aabb[0], 1.0),
                worldTransform * glm::vec4(mesh.aabb[1], 1.0)
            };

            // if the frustrum can't see the mesh's OBB we cull it
            if (!frustrum.vsAABB(worldAABB[0], worldAABB[1])) {
                culled += 1;
                continue;
            }


        ECS::MaterialComponent* material = scene.materials.getComponent(entity);

        if (material) {
            if(material->albedo) material->albedo->bindToSlot(0);
            if(material->normals) material->normals->bindToSlot(3);
        }

        if (transform) {
            shader.getUniform("model") = transform->matrix;
        }
        else {
            shader.getUniform("model") = glm::mat4(1.0f);
        }

        // write the entity ID to the stencil buffer for picking
        glStencilFunc(GL_ALWAYS, (GLint)entity, 0xFFFF);

        mesh.vertexBuffer.bind();
        mesh.indexBuffer.bind();
        Renderer::DrawIndexed(mesh.indexBuffer.count);
    }

    GBuffer.unbind();

    // disable stencil stuff
    glStencilFunc(GL_ALWAYS, 0, 0xFFFF);  // Set any stencil to 0
    glDisable(GL_STENCIL_TEST);
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

void DeferredLighting::execute(Scene& sscene, Viewport& viewport, ShadowMap* shadowMap, OmniShadowMap* omniShadowMap, 
                                GeometryBuffer* GBuffer, ScreenSpaceAmbientOcclusion* ambientOcclusion, Voxelization* voxels, Mesh* quad) {
    hotloader.checkForUpdates();
    
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

    shader.getUniform("pointLightCount") = (uint32_t)sscene.pointLights.getCount();
    shader.getUniform("directionalLightCount") = (uint32_t)sscene.directionalLights.getCount();

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

    // update the uniform buffer CPU side
    uniforms.view = viewport.getCamera().getView();
    uniforms.projection = viewport.getCamera().getProjection();

    // update every light type
    for (uint32_t i = 0; i < sscene.directionalLights.getCount() && i < ARRAYSIZE(uniforms.dirLights); i++) {
        auto entity = sscene.directionalLights.getEntity(i);
        auto transform = sscene.transforms.getComponent(entity);

        auto& light = sscene.directionalLights[i];
        light.buffer.direction = shadowMap->sunCamera.getDirection();
        uniforms.dirLights[i] = light.buffer;
    }

    for (uint32_t i = 0; i < sscene.pointLights.getCount() && i < ARRAYSIZE(uniforms.pointLights); i++) {
        // TODO: might want to move the code for updating every light with its transform to a system
        // instead of doing it here
        auto entity = sscene.pointLights.getEntity(i);
        auto transform = sscene.transforms.getComponent(entity);

        auto& light = sscene.pointLights[i];

        light.buffer.position = glm::vec4(transform->position, 1.0f);

        uniforms.pointLights[i] = light.buffer;
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

    // Direct State Access (TODO: Experimental, implement everywhere)
    result.init(size, size, size, GL_RGBA8, nullptr);
    result.setFilter(Sampling::Filter::Trilinear);
    result.genMipMaps();
    
    // left, right, bottom, top, zNear, zFar
    auto projectionMatrix = glm::ortho(-worldSize * 0.5f, worldSize * 0.5f, -worldSize * 0.5f, worldSize * 0.5f, worldSize * 0.5f, worldSize * 1.5f);
    px = projectionMatrix * glm::lookAt(glm::vec3(worldSize, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    py = projectionMatrix * glm::lookAt(glm::vec3(0, worldSize, 0), glm::vec3(0, 0, 0), glm::vec3(0, 0, -1));
    pz = projectionMatrix * glm::lookAt(glm::vec3(0, 0, worldSize), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
}


void Voxelization::execute(Scene& scene, Viewport& viewport, ShadowMap* shadowmap) {
    hotloader.checkForUpdates();

    result.clear({ 0, 0, 0, 0 });

    // set GL state
    glViewport(0, 0, size, size);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // bind shader and 3d voxel map
    shader.bind();
    result.bindToSlot(1, GL_WRITE_ONLY, GL_RGBA8);
    shadowmap->result.bindToSlot(2);

    shader.getUniform("lightViewProjection") = shadowmap->sunCamera.getProjection() * shadowmap->sunCamera.getView();


    for (uint32_t i = 0; i < scene.meshes.getCount(); i++) {
        ECS::Entity entity = scene.meshes.getEntity(i);

        ECS::MeshComponent& mesh = scene.meshes[i];
        ECS::TransformComponent* transform = scene.transforms.getComponent(entity);
        ECS::MaterialComponent* material = scene.materials.getComponent(entity);

        shader.getUniform("model") = transform ? transform->matrix : glm::mat4(1.0f);
        shader.getUniform("px") = px;
        shader.getUniform("py") = py;
        shader.getUniform("pz") = pz;

        if (material) {
            if (material->albedo) material->albedo->bindToSlot(0);
        }

        mesh.vertexBuffer.bind();
        mesh.indexBuffer.bind();
        Renderer::DrawIndexed(mesh.indexBuffer.count);
    }

    // sync with host and generate mips
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    result.genMipMaps();

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

    renderBuffer.init(viewport.size.x, viewport.size.y, GL_DEPTH32F_STENCIL8);

    frameBuffer.attach(renderBuffer, GL_DEPTH_STENCIL_ATTACHMENT);

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
    {"BINORMAL",    ShaderType::FLOAT3}
        });
}

void BoundingBoxDebug::execute(Scene& scene, Viewport& viewport, glTexture2D& texture, ECS::Entity active) {
    if (!active) return;
    ECS::MeshComponent* mesh = scene.meshes.getComponent(active);
    ECS::TransformComponent* transform = scene.transforms.getComponent(active);
    if (!mesh) return;

    glEnable(GL_LINE_SMOOTH);

    frameBuffer.bind();
    frameBuffer.attach(texture, GL_COLOR_ATTACHMENT0);
    glClear(GL_DEPTH_BUFFER_BIT);

    shader.bind();
    shader.getUniform("projection") = viewport.getCamera().getProjection();
    shader.getUniform("view") = viewport.getCamera().getView();
    shader.getUniform("model") = transform->matrix;

    // calculate obb from aabb
    const auto min = mesh->aabb[0];
    const auto max = mesh->aabb[1];
    
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

    renderBuffer.init(viewport.size.x, viewport.size.y, GL_DEPTH32F_STENCIL8);
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

void ForwardLightingPass::execute(Viewport& viewport, Scene& scene, Voxelization* voxels, ShadowMap* shadowmap) {
    hotloader.checkForUpdates();

    // enable stencil stuff
    glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glStencilMask(0xFFFF); // Write to stencil buffer
    glStencilFunc(GL_ALWAYS, 0, 0xFFFF);  // Set any stencil to 0

    // update the uniform buffer CPU side
    uniforms.view = viewport.getCamera().getView();
    uniforms.projection = viewport.getCamera().getProjection();

    for (uint32_t i = 0; i < scene.directionalLights.getCount() && i < ARRAYSIZE(uniforms.dirLights); i++) {
        auto entity = scene.directionalLights.getEntity(i);
        auto transform = scene.transforms.getComponent(entity);

        auto& light = scene.directionalLights[i];
        light.buffer.direction = shadowmap->sunCamera.getDirection();

        uniforms.dirLights[i] = light.buffer;
    }

    for (uint32_t i = 0; i < scene.pointLights.getCount() && i < ARRAYSIZE(uniforms.pointLights); i++) {
        // TODO: might want to move the code for updating every light with its transform to a system
        // instead of doing it here
        auto entity = scene.pointLights.getEntity(i);
        auto transform = scene.transforms.getComponent(entity);

        auto& light = scene.pointLights[i];

        light.buffer.position = glm::vec4(transform->position, 1.0f);

        uniforms.pointLights[i] = light.buffer;
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

    for (uint32_t i = 0; i < scene.meshes.getCount(); i++) {
        ECS::Entity entity = scene.meshes.getEntity(i);

        ECS::MeshComponent& mesh = scene.meshes[i];

        ECS::NameComponent* name = scene.names.getComponent(entity);

        ECS::TransformComponent* transform = scene.transforms.getComponent(entity);
        const glm::mat4& worldTransform = transform ? transform->matrix : glm::mat4(1.0f);

        // convert AABB from local to world space
        std::array<glm::vec3, 2> worldAABB = {
            worldTransform * glm::vec4(mesh.aabb[0], 1.0),
            worldTransform * glm::vec4(mesh.aabb[1], 1.0)
        };

        // if the frustrum can't see the mesh's OBB we cull it
        if (!frustrum.vsAABB(worldAABB[0], worldAABB[1])) {
            culled += 1;
            continue;
        }

        ECS::MaterialComponent* material = scene.materials.getComponent(entity);

        if (material) {
            if (material->albedo) material->albedo->bindToSlot(1);
            if (material->normals) material->normals->bindToSlot(2);
        }

        if (transform) {
            shader.getUniform("model") = transform->matrix;
        }
        else {
            shader.getUniform("model") = glm::mat4(1.0f);
        }

        // write the entity ID to the stencil buffer for picking
        glStencilFunc(GL_ALWAYS, (GLint)entity, 0xFFFF);

        mesh.vertexBuffer.bind();
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

} // renderpass
} // namespace Raekor