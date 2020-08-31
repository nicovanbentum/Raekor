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
    glCreateTextures(GL_TEXTURE_2D, 1, &result);
    glTextureStorage2D(result, 1, GL_DEPTH_COMPONENT32F, width, height);
    
    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(result, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(result, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(result, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTextureParameteri(result, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTextureParameteri(result, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);

    glCreateFramebuffers(1, &framebuffer);
    glNamedFramebufferTexture(framebuffer, GL_DEPTH_ATTACHMENT, result, 0);
    glNamedFramebufferDrawBuffer(framebuffer, GL_NONE);
    glNamedFramebufferReadBuffer(framebuffer, GL_NONE);
}

ShadowMap::~ShadowMap() {
    glDeleteTextures(1, &result);
    glDeleteFramebuffers(1, &framebuffer);
}

void ShadowMap::execute(entt::registry& scene) {
    // setup the shadow map 
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glClear(GL_DEPTH_BUFFER_BIT);
    glCullFace(GL_FRONT);

    // render the entire scene to the directional light shadow map
    auto lightView = scene.view<ecs::DirectionalLightComponent, ecs::TransformComponent>();
    auto lookDirection = glm::vec3(0.0f, -1.0f, 0.0f);
    
    if (!lightView.empty()) {
        auto& lightTransform = lightView.get<ecs::TransformComponent>(lightView.front());
        lookDirection = static_cast<glm::quat>(lightTransform.rotation) * lookDirection;
    } else {
        // we rotate default light a little or else we get nan values in our view matrix
        lookDirection = static_cast<glm::quat>(glm::vec3(glm::radians(15.0f), 0, 0)) * lookDirection;
    }

    lookDirection = glm::clamp(lookDirection, { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f });

    // TODO: calculate these matrices based on the view frustrum
    auto mapView = glm::lookAtRH(
        glm::vec3(0.0f, 15.0f, 0.0f),
        glm::vec3(0.0f, 15.0f, 0.0f) + lookDirection,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    auto mapProjection = glm::orthoRH_ZO(
        -settings.size, settings.size, -settings.size, settings.size,
        settings.planes.x, settings.planes.y
    );

    shader.bind();
    uniforms.cameraMatrix = mapProjection * mapView;
    uniformBuffer.update(&uniforms, sizeof(uniforms));
    uniformBuffer.bind(0);

    auto view = scene.view<ecs::MeshComponent, ecs::TransformComponent>();

    for (auto entity : view) {
        auto& mesh = view.get<ecs::MeshComponent>(entity);
        auto& transform = view.get<ecs::TransformComponent>(entity);

        shader.getUniform("model") = transform.worldTransform;

        // determine if we use the original mesh vertices or GPU skinned vertices
        if (scene.has<ecs::MeshAnimationComponent>(entity)) {
            scene.get<ecs::MeshAnimationComponent>(entity).skinnedVertexBuffer.bind();
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

        auto view = scene.view<ecs::MeshComponent, ecs::TransformComponent>();

        for (auto entity : view) {
            auto& mesh = view.get<ecs::MeshComponent>(entity);
            auto& transform = view.get<ecs::TransformComponent>(entity);

            shader.getUniform("model") = transform.worldTransform;

            // determine if we use the original mesh vertices or GPU skinned vertices
            if (scene.has<ecs::MeshAnimationComponent>(entity)) {
                scene.get<ecs::MeshAnimationComponent>(entity).skinnedVertexBuffer.bind();
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
    shader.reload(gbufferStages.data(), gbufferStages.size());
    hotloader.watch(&shader, gbufferStages.data(), gbufferStages.size());

    createResources(viewport);
}

void GeometryBuffer::execute(entt::registry& scene, Viewport& viewport) {
    hotloader.checkForUpdates();

    glBindFramebuffer(GL_FRAMEBUFFER, GBuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader.bind();
    shader.getUniform("projection") = viewport.getCamera().getProjection();
    shader.getUniform("view") = viewport.getCamera().getView();

    Math::Frustrum frustrum;
    frustrum.update(viewport.getCamera().getProjection() * viewport.getCamera().getView(), true);

    culled = 0;

    auto view = scene.view<ecs::MeshComponent, ecs::TransformComponent>();

    for (auto entity : view) {
        auto& mesh = view.get<ecs::MeshComponent>(entity);
        auto& transform = view.get<ecs::TransformComponent>(entity);

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


        auto material = scene.try_get<ecs::MaterialComponent>(mesh.material);

        if (material) {
            if (material->albedo)       material->albedo->bindToSlot(0);
            if (material->normals)      material->normals->bindToSlot(3);
            if (material->metalrough)   material->metalrough->bindToSlot(4);
        }

        shader.getUniform("model") = transform.worldTransform;

        shader.getUniform("entity") = entt::to_integral(entity);

        // determine if we use the original mesh vertices or GPU skinned vertices
        if (scene.has<ecs::MeshAnimationComponent>(entity)) {
            scene.get<ecs::MeshAnimationComponent>(entity).skinnedVertexBuffer.bind();
        } else {
            mesh.vertexBuffer.bind();
        }

        mesh.indexBuffer.bind();
        glDrawElements(GL_TRIANGLES, mesh.indexBuffer.count, GL_UNSIGNED_INT, nullptr);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

entt::entity GeometryBuffer::pick(uint32_t x, uint32_t y) {
    glm::vec4 readPixel;
    glGetTextureSubImage(materialTexture, 0, x, y, 0, 1, 1, 1, GL_RGBA, GL_FLOAT, sizeof(glm::vec4), glm::value_ptr(readPixel));
    return static_cast<entt::entity>(readPixel.b);
}

void GeometryBuffer::createResources(Viewport& viewport) {
    glCreateTextures(GL_TEXTURE_2D, 1, &albedoTexture);
    glTextureStorage2D(albedoTexture, 1, GL_RGBA16F, viewport.size.x, viewport.size.y);
    glTextureParameteri(albedoTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(albedoTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateTextures(GL_TEXTURE_2D, 1, &normalTexture);
    glTextureStorage2D(normalTexture, 1, GL_RGBA16F, viewport.size.x, viewport.size.y);
    glTextureParameteri(albedoTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(albedoTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateTextures(GL_TEXTURE_2D, 1, &positionTexture);
    glTextureStorage2D(positionTexture, 1, GL_RGBA16F, viewport.size.x, viewport.size.y);
    glTextureParameteri(positionTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(positionTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(positionTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(positionTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(positionTexture, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glCreateTextures(GL_TEXTURE_2D, 1, &materialTexture);
    glTextureStorage2D(materialTexture, 1, GL_RGBA32F, viewport.size.x, viewport.size.y);
    glTextureParameteri(positionTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(positionTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateRenderbuffers(1, &GDepthBuffer);
    glNamedRenderbufferStorage(GDepthBuffer, GL_DEPTH_COMPONENT32F, viewport.size.x, viewport.size.y);

    glCreateFramebuffers(1, &GBuffer);
    glNamedFramebufferTexture(GBuffer, GL_COLOR_ATTACHMENT0, positionTexture, 0);
    glNamedFramebufferTexture(GBuffer, GL_COLOR_ATTACHMENT1, normalTexture, 0);
    glNamedFramebufferTexture(GBuffer, GL_COLOR_ATTACHMENT2, albedoTexture, 0);
    glNamedFramebufferTexture(GBuffer, GL_COLOR_ATTACHMENT3, materialTexture, 0);

    std::array<GLenum, 4> colorAttachments = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
    glNamedFramebufferDrawBuffers(GBuffer, static_cast<GLsizei>(colorAttachments.size()), colorAttachments.data());

    glNamedFramebufferRenderbuffer(GBuffer, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, GDepthBuffer);
}

void GeometryBuffer::deleteResources() {
    std::array<unsigned int, 4> textures = { albedoTexture, normalTexture, positionTexture, materialTexture };
    glDeleteTextures(textures.size(), textures.data());
    glDeleteRenderbuffers(1, &GDepthBuffer);
    glDeleteFramebuffers(1, &GBuffer);
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

    createResources(viewport);
}

void ScreenSpaceAmbientOcclusion::execute(Viewport& viewport, GeometryBuffer* geometryPass, Mesh* quad) {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glBindTextureUnit(0, geometryPass->positionTexture);
    glBindTextureUnit(1, geometryPass->normalTexture);
    glBindTextureUnit(2, noiseTexture);
    
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
    glBindFramebuffer(GL_FRAMEBUFFER, blurFramebuffer);
    glBindTextureUnit(0, preblurResult);
    blurShader.bind();

    quad->render();
}

void ScreenSpaceAmbientOcclusion::createResources(Viewport& viewport) {
    // create SSAO kernel hemisphere
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
    std::default_random_engine generator;

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
    glCreateTextures(GL_TEXTURE_2D, 1, &noiseTexture);
    glTextureStorage2D(noiseTexture, 1, GL_RGB16F, 4, 4);
    glTextureSubImage2D(noiseTexture, 0, 0, 0, 4, 4, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
    glTextureParameteri(noiseTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(noiseTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(noiseTexture, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(noiseTexture, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(noiseTexture, GL_TEXTURE_WRAP_R, GL_REPEAT);

    glCreateTextures(GL_TEXTURE_2D, 1, &preblurResult);
    glTextureStorage2D(preblurResult, 1, GL_RGBA, viewport.size.x, viewport.size.y);
    glTextureParameteri(preblurResult, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(preblurResult, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateFramebuffers(1, &framebuffer);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT0, preblurResult, 0);
    glNamedFramebufferDrawBuffer(framebuffer, GL_COLOR_ATTACHMENT0);

    glCreateTextures(GL_TEXTURE_2D, 1, &result);
    glTextureStorage2D(result, 1, GL_RGBA, viewport.size.x, viewport.size.y);
    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateFramebuffers(1, &blurFramebuffer);
    glNamedFramebufferTexture(blurFramebuffer, GL_COLOR_ATTACHMENT0, result, 0);
    glNamedFramebufferDrawBuffer(blurFramebuffer, GL_COLOR_ATTACHMENT0);
}

void ScreenSpaceAmbientOcclusion::deleteResources() {
    std::array<GLuint, 3> textures = { noiseTexture, result, preblurResult };
    glDeleteTextures(textures.size(), textures.data());

    glDeleteFramebuffers(1, &framebuffer);
    glDeleteFramebuffers(1, &blurFramebuffer);
}

//////////////////////////////////////////////////////////////////////////////////

DeferredLighting::DeferredLighting(Viewport& viewport) {
    // load shaders from disk

    Shader::Stage vertex(Shader::Type::VERTEX, "shaders\\OpenGL\\main.vert");
    Shader::Stage frag(Shader::Type::FRAG, "shaders\\OpenGL\\main.frag");
    std::array<Shader::Stage, 2> modelStages = { vertex, frag };
    shader.reload(modelStages.data(), modelStages.size());

    hotloader.watch(&shader, modelStages.data(), modelStages.size());

    // init resources
    createResources(viewport);

    // init uniform buffer
    uniformBuffer.setSize(sizeof(uniforms));
}

void DeferredLighting::execute(entt::registry& sscene, Viewport& viewport, ShadowMap* shadowMap, OmniShadowMap* omniShadowMap,
                                GeometryBuffer* GBuffer, ScreenSpaceAmbientOcclusion* ambientOcclusion, Voxelization* voxels, Mesh* quad) {
    hotloader.checkForUpdates();

    // bind the main framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glClear(GL_COLOR_BUFFER_BIT);

    // set uniforms
    shader.bind();
    shader.getUniform("sunColor") = settings.sunColor;
    shader.getUniform("minBias") = settings.minBias;
    shader.getUniform("maxBias") = settings.maxBias;
    shader.getUniform("farPlane") = settings.farPlane;
    shader.getUniform("bloomThreshold") = settings.bloomThreshold;

    shader.getUniform("pointLightCount") = static_cast<uint32_t>(sscene.view<ecs::PointLightComponent>().size());
    shader.getUniform("directionalLightCount") = static_cast<uint32_t>(sscene.view<ecs::DirectionalLightComponent>().size());

    shader.getUniform("voxelsWorldSize") = voxels->worldSize;

    // bind textures to shader binding slots
    glBindTextureUnit(0, shadowMap->result);
    
    if (omniShadowMap) {
        omniShadowMap->result.bindToSlot(1);
    }

    glBindTextureUnit(2, GBuffer->positionTexture);
    glBindTextureUnit(3, GBuffer->albedoTexture);
    glBindTextureUnit(4, GBuffer->normalTexture);

    if (ambientOcclusion) {
        glBindTextureUnit(5, ambientOcclusion->result);
    }

    voxels->result.bindToSlot(6);
    glBindTextureUnit(7, GBuffer->materialTexture);

    // update the uniform buffer CPU side
    uniforms.view = viewport.getCamera().getView();
    uniforms.projection = viewport.getCamera().getProjection();

    // update every light type
    // TODO: figure out this directional light crap, I only really want to support a single one or figure out a better way to deal with this
    // For now we send only the first directional light to the GPU for everything, if none are present we send a buffer with a direction of (0, -1, 0)
    {
        auto view = sscene.view<ecs::DirectionalLightComponent, ecs::TransformComponent>();
        auto entity = view.front();
        if (entity != entt::null) {
            auto& light = view.get<ecs::DirectionalLightComponent>(entity);
            auto& transform = view.get<ecs::TransformComponent>(entity);

            light.buffer.direction = glm::vec4(static_cast<glm::quat>(transform.rotation) * glm::vec3(0, -1, 0), 1.0);
            uniforms.dirLights[0] = light.buffer;
        }  else {
            auto light = ecs::DirectionalLightComponent();
            light.buffer.direction.y = -0.9f;
            uniforms.dirLights[0] = light.buffer;
        }
    }

    auto posView = sscene.view<ecs::PointLightComponent, ecs::TransformComponent>();
    unsigned int posViewCounter = 0;
    for (auto entity : posView) {
        auto& light = posView.get<ecs::PointLightComponent>(entity);
        auto& transform = posView.get<ecs::TransformComponent>(entity);

        posViewCounter++;
        if (posViewCounter >= ARRAYSIZE(uniforms.pointLights)) {
            break;
        }

        light.buffer.position = glm::vec4(transform.position, 1.0f);
        uniforms.pointLights[posViewCounter] = light.buffer;
    }

    uniforms.cameraPosition = glm::vec4(viewport.getCamera().getPosition(), 1.0);
    uniforms.lightSpaceMatrix = shadowMap->uniforms.cameraMatrix;

    // update uniform buffer GPU side
    uniformBuffer.update(&uniforms, sizeof(uniforms));
    uniformBuffer.bind(0);

    // perform lighting pass and unbind
    quad->render();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DeferredLighting::createResources(Viewport& viewport) {
    // init render targets
    glCreateTextures(GL_TEXTURE_2D, 1, &result);
    glTextureStorage2D(result, 1, GL_RGBA16F, viewport.size.x, viewport.size.y);
    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glCreateTextures(GL_TEXTURE_2D, 1, &bloomHighlights);
    glTextureStorage2D(bloomHighlights, 1, GL_RGBA16F, viewport.size.x, viewport.size.y);
    glTextureParameteri(bloomHighlights, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(bloomHighlights, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glCreateFramebuffers(1, &framebuffer);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT0, result, 0);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT1, bloomHighlights, 0);
}

void DeferredLighting::deleteResources() {
    glDeleteTextures(1, &result);
    glDeleteTextures(1, &bloomHighlights);
    glDeleteFramebuffers(1, &framebuffer);
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
}

void Bloom::execute(glTexture2D& scene, glTexture2D& highlights, Mesh* quad) {
    // perform gaussian blur on the bloom texture using "ping pong" framebuffers that
    // take each others color attachments as input and perform a directional gaussian blur each
    // iteration
    bool horizontal = true, firstIteration = true;
    blurShader.bind();
    for (unsigned int i = 0; i < 10; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, blurBuffers[horizontal]);
        blurShader.getUniform("horizontal") = horizontal;
        if (firstIteration) {
            highlights.bindToSlot(0);
            firstIteration = false;
        }
        else {
            glBindTextureUnit(0, blurTextures[!horizontal]);
        }
        quad->render();
        horizontal = !horizontal;
    }

    blurShader.unbind();

    // blend the bloom and scene texture together
    glBindFramebuffer(GL_FRAMEBUFFER, resultFramebuffer);
    bloomShader.bind();
    scene.bindToSlot(0);
    glBindTextureUnit(1, blurTextures[!horizontal]);
    quad->render();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Bloom::createResources(Viewport& viewport) {
    // init render targets
    glCreateTextures(GL_TEXTURE_2D, 1, &result);
    glTextureStorage2D(result, 1, GL_RGBA16F, viewport.size.x, viewport.size.y);
    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glCreateFramebuffers(1, &resultFramebuffer);
    glNamedFramebufferTexture(resultFramebuffer, GL_COLOR_ATTACHMENT0, result, 0);
    glNamedFramebufferDrawBuffer(resultFramebuffer, GL_COLOR_ATTACHMENT0);

    for (unsigned int i = 0; i < 2; i++) {
        glCreateTextures(GL_TEXTURE_2D, 1, &blurTextures[i]);
        glTextureStorage2D(blurTextures[i], 1, GL_RGBA16F, viewport.size.x, viewport.size.y);
        glTextureParameteri(blurTextures[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(blurTextures[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(blurTextures[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(blurTextures[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(blurTextures[i], GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glCreateFramebuffers(1, &blurBuffers[i]);
        glNamedFramebufferTexture(blurBuffers[i], GL_COLOR_ATTACHMENT0, blurTextures[i], 0);
        glNamedFramebufferDrawBuffer(blurBuffers[i], GL_COLOR_ATTACHMENT0);
    }
}

void Bloom::deleteResources() {
    glDeleteTextures(1, &result);
    glDeleteTextures(2, blurTextures);
    glDeleteFramebuffers(2, blurBuffers);
    glDeleteFramebuffers(1, &resultFramebuffer);
}

//////////////////////////////////////////////////////////////////////////////////

Tonemapping::Tonemapping(Viewport& viewport) {
    // load shaders from disk
    std::vector<Shader::Stage> tonemapStages;
    tonemapStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\HDR.vert");
    tonemapStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\HDR.frag");
    shader.reload(tonemapStages.data(), tonemapStages.size());

    // init render targets
    createResources(viewport);

    // init uniform buffer
    uniformBuffer.setSize(sizeof(settings));
}

void Tonemapping::execute(unsigned int scene, Mesh* quad) {
    // bind and clear render target
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // bind shader and input texture
    shader.bind();
    glBindTextureUnit(0, scene);

    // update uniform buffer GPU side
    uniformBuffer.update(&settings, sizeof(settings));
    uniformBuffer.bind(0);

    // render fullscreen quad to perform tonemapping
    quad->render();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Tonemapping::createResources(Viewport& viewport) {
    // init render targets
    glCreateTextures(GL_TEXTURE_2D, 1, &result);
    glTextureStorage2D(result, 1, GL_RGB8, viewport.size.x, viewport.size.y);
    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateFramebuffers(1, &framebuffer);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT0, result, 0);
    glNamedFramebufferDrawBuffer(framebuffer, GL_COLOR_ATTACHMENT0);
}

void Tonemapping::deleteResources() {
    glDeleteTextures(1, &result);
    glDeleteFramebuffers(1, &framebuffer);
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
    glBindTextureUnit(2, shadowmap->result);

    shader.getUniform("lightViewProjection") = shadowmap->uniforms.cameraMatrix;


    auto view = scene.view<ecs::MeshComponent, ecs::TransformComponent>();

    for (auto entity : view) {

        auto& mesh = view.get<ecs::MeshComponent>(entity);
        auto& transform = view.get<ecs::TransformComponent>(entity);

        ecs::MaterialComponent* material = scene.try_get<ecs::MaterialComponent>(mesh.material);

        shader.getUniform("model") = transform.worldTransform;
        shader.getUniform("px") = px;
        shader.getUniform("py") = py;
        shader.getUniform("pz") = pz;

        if (material) {
            if (material->albedo) material->albedo->bindToSlot(0);
        }

        // determine if we use the original mesh vertices or GPU skinned vertices
        if (scene.has<ecs::MeshAnimationComponent>(entity)) {
            scene.get<ecs::MeshAnimationComponent>(entity).skinnedVertexBuffer.bind();
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

    // init resources
    createResources(viewport);
}

void VoxelizationDebug::execute(Viewport& viewport, unsigned int input, Voxelization* voxels) {
    // bind the input framebuffer, we draw the debug vertices on top
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
    glNamedFramebufferTexture(frameBuffer, GL_COLOR_ATTACHMENT0, input, 0);
    glNamedFramebufferDrawBuffer(frameBuffer, GL_COLOR_ATTACHMENT0);
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

void VoxelizationDebug::createResources(Viewport& viewport) {
    // init resources
    glCreateRenderbuffers(1, &renderBuffer);
    glNamedRenderbufferStorage(renderBuffer, GL_DEPTH32F_STENCIL8, viewport.size.x, viewport.size.y);

    glCreateFramebuffers(1, &frameBuffer);
    glNamedFramebufferRenderbuffer(frameBuffer, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderBuffer);
}

void VoxelizationDebug::deleteResources() {
    glDeleteRenderbuffers(1, &renderBuffer);
    glDeleteFramebuffers(1, &frameBuffer);
}

//////////////////////////////////////////////////////////////////////////////////

BoundingBoxDebug::BoundingBoxDebug(Viewport& viewport) {
    // load shaders from disk
    std::vector<Shader::Stage> aabbStages;
    aabbStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\aabb.vert");
    aabbStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\aabb.frag");
    shader.reload(aabbStages.data(), aabbStages.size());

    glCreateTextures(GL_TEXTURE_2D, 1, &result);
    glTextureStorage2D(result, 1, GL_RGBA32F, viewport.size.x, viewport.size.y);
    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateFramebuffers(1, &frameBuffer);

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

void BoundingBoxDebug::execute(entt::registry& scene, Viewport& viewport, unsigned int texture, unsigned int renderBuffer, entt::entity active) {
    
    assert(active != entt::null);
    if (!scene.has<ecs::MeshComponent>(active) || !scene.has<ecs::TransformComponent>(active)) {
        return;
    }

    auto& mesh = scene.get<ecs::MeshComponent>(active);
    auto& transform = scene.get<ecs::TransformComponent>(active);

    glEnable(GL_LINE_SMOOTH);

    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
    glNamedFramebufferTexture(frameBuffer, GL_COLOR_ATTACHMENT0, texture, 0);
    glNamedFramebufferDrawBuffer(frameBuffer, GL_COLOR_ATTACHMENT0);
    glNamedFramebufferRenderbuffer(frameBuffer, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderBuffer);

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
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void BoundingBoxDebug::createResources(Viewport& viewport) {
    glCreateTextures(GL_TEXTURE_2D, 1, &result);
    glTextureStorage2D(result, 1, GL_RGBA32F, viewport.size.x, viewport.size.y);
    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateFramebuffers(1, &frameBuffer);
}

void BoundingBoxDebug::deleteResources() {
    glDeleteTextures(1, &result);
    glDeleteFramebuffers(1, &frameBuffer);
}

ForwardLightingPass::ForwardLightingPass(Viewport& viewport) {
    // load shaders from disk
    std::vector<Shader::Stage> stages;
    stages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\fwdLight.vert");
    stages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\fwdLight.frag");
    shader.reload(stages.data(), stages.size());

    hotloader.watch(&shader, stages.data(), stages.size());

    // init render targets
    createResources(viewport);

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
    auto dirView = scene.view<ecs::DirectionalLightComponent, ecs::TransformComponent>();
    unsigned int dirLightCounter = 0;
    for (auto entity : dirView) {
        auto& light = dirView.get<ecs::DirectionalLightComponent>(entity);
        auto& transform = dirView.get<ecs::TransformComponent>(entity);

        dirLightCounter++;
        if (dirLightCounter >= ARRAYSIZE(uniforms.dirLights)) {
            break;
        }

        light.buffer.direction = glm::vec4(static_cast<glm::quat>(transform.rotation) * glm::vec3(0, -1, 0), 1.0);
        uniforms.dirLights[0] = light.buffer;
        uniforms.dirLights[dirLightCounter] = light.buffer;
    }

    auto posView = scene.view<ecs::PointLightComponent, ecs::TransformComponent>();
    unsigned int posViewCounter = 0;
    for (auto entity : posView) {
        auto& light = posView.get<ecs::PointLightComponent>(entity);
        auto& transform = posView.get<ecs::TransformComponent>(entity);

        posViewCounter++;
        if (posViewCounter >= ARRAYSIZE(uniforms.pointLights)) {
            break;
        }

        light.buffer.position = glm::vec4(transform.position, 1.0f);
        uniforms.pointLights[posViewCounter] = light.buffer;
    }

    uniforms.cameraPosition = glm::vec4(viewport.getCamera().getPosition(), 1.0);
    uniforms.lightSpaceMatrix = shadowmap->uniforms.cameraMatrix;

    // update uniform buffer GPU side
    uniformBuffer.update(&uniforms, sizeof(uniforms));

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    shader.bind();
    uniformBuffer.bind(0);

    voxels->result.bindToSlot(0);
    glBindTextureUnit(3, shadowmap->result);

    Math::Frustrum frustrum;
    frustrum.update(viewport.getCamera().getProjection() * viewport.getCamera().getView(), true);
    culled = 0;

    auto view = scene.view<ecs::MeshComponent, ecs::TransformComponent>();

    for (auto entity : view) {
        auto& mesh = scene.get<ecs::MeshComponent>(entity);
        auto& transform = scene.get<ecs::TransformComponent>(entity);

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

        ecs::MaterialComponent* material = scene.try_get<ecs::MaterialComponent>(mesh.material);

        if (material) {
            if (material->albedo) material->albedo->bindToSlot(1);
            if (material->normals) material->normals->bindToSlot(2);
        }

        shader.getUniform("model") = transform.worldTransform;

        // write the entity ID to the stencil buffer for picking
        glStencilFunc(GL_ALWAYS, (GLint)entity, 0xFFFF);

        // determine if we use the original mesh vertices or GPU skinned vertices
        if (scene.has<ecs::MeshAnimationComponent>(entity)) {
            scene.get<ecs::MeshAnimationComponent>(entity).skinnedVertexBuffer.bind();
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
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ForwardLightingPass::createResources(Viewport& viewport) {
    // init render targets
    glCreateTextures(GL_TEXTURE_2D, 1, &result);
    glTextureStorage2D(result, 1, GL_RGBA16F, viewport.size.x, viewport.size.y);
    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glCreateRenderbuffers(1, &renderbuffer);
    glNamedRenderbufferStorage(renderbuffer, GL_DEPTH32F_STENCIL8, viewport.size.x, viewport.size.y);

    glCreateFramebuffers(1, &framebuffer);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT0, result, 0);
    glNamedFramebufferRenderbuffer(framebuffer, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);
}

void ForwardLightingPass::deleteResources() {
    glDeleteTextures(1, &result);
    glDeleteRenderbuffers(1, &renderbuffer);
    glDeleteFramebuffers(1, &framebuffer);
}

SkyPass::SkyPass(Viewport& viewport) {
    std::vector<glShader::Stage> stages;
    stages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\sky.vert");
    stages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\sky.frag");
    shader.reload(stages.data(), stages.size());
    hotloader.watch(&shader, stages.data(), stages.size());

    glCreateTextures(GL_TEXTURE_2D, 1, &result);
    glTextureStorage2D(result, 1, GL_RGBA32F, viewport.size.x, viewport.size.y);
    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(result, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(result, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(result, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glCreateFramebuffers(1, &framebuffer);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT0, result, 0);
}

void SkyPass::execute(Viewport& viewport, Mesh* quad) {
    hotloader.checkForUpdates();

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glClear(GL_COLOR_BUFFER_BIT);

    shader.bind();
    shader.getUniform("projection") = viewport.getCamera().getProjection();
    shader.getUniform("view") = viewport.getCamera().getView();
    shader.getUniform("time") = settings.time;
    shader.getUniform("cirrus") = settings.cirrus;
    shader.getUniform("cumulus") = settings.cumulus;

    quad->render();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Skinning::execute(ecs::MeshComponent& mesh, ecs::MeshAnimationComponent& anim) {

    glNamedBufferData(anim.boneTransformsBuffer, anim.boneTransforms.size() * sizeof(glm::mat4), anim.boneTransforms.data(), GL_DYNAMIC_DRAW);

    computeShader.bind();

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, anim.boneIndexBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, anim.boneWeightBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, mesh.vertexBuffer.id);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, anim.skinnedVertexBuffer.id);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, anim.boneTransformsBuffer);

    glDispatchCompute(static_cast<GLuint>(mesh.vertices.size()), 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void EnvironmentPass::execute(const std::string& file, Mesh* unitCube) {
    stbi_set_flip_vertically_on_load(true);
    int w, h, ch;
    float* data = stbi_loadf(file.c_str(), &w, &h, &ch, 3);
    if (!data) return;

    unsigned int originalTexture;
    glGenTextures(1, &originalTexture);
    glBindTexture(GL_TEXTURE_2D, originalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);

    captureRenderbuffer.init(512, 512, GL_DEPTH_COMPONENT24);
    captureFramebuffer.attach(captureRenderbuffer, GL_DEPTH_ATTACHMENT);

    for (unsigned int i = 0; i < 6; i++) {
        envCubemap.init(512, 512, i, { GL_RGB16F, GL_RGB, GL_FLOAT }, nullptr);
    }

    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    std::array<glm::mat4, 6> captureViews = {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    toCubemapShader.bind();
    toCubemapShader.getUniform("projection") = captureProjection;
    glBindTextureUnit(originalTexture, 0);

    glViewport(0, 0, 512, 512);
    captureFramebuffer.bind();
    for (unsigned int i = 0; i < 6; i++) {
        toCubemapShader["view"] = captureViews[i];
        captureFramebuffer.attach(envCubemap, GL_COLOR_ATTACHMENT0 + i, i);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        unitCube->render();
    }



}

} // renderpass
} // namespace Raekor