#include "pch.h"
#include "renderpass.h"
#include "ecs.h"
#include "camera.h"
#include "timer.h"
#include "scene.h"

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
    glTextureParameteri(result, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(result, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTextureParameteri(result, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTextureParameterfv(result, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTextureParameteri(result, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTextureParameteri(result, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);

    glCreateFramebuffers(1, &framebuffer);
    glNamedFramebufferTexture(framebuffer, GL_DEPTH_ATTACHMENT, result, 0);
    glNamedFramebufferDrawBuffer(framebuffer, GL_NONE);
    glNamedFramebufferReadBuffer(framebuffer, GL_NONE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

ShadowMap::~ShadowMap() {
    glDeleteTextures(1, &result);
    glDeleteFramebuffers(1, &framebuffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

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

        // convert AABB from local to world space
        std::array<glm::vec3, 2> worldAABB = {
            transform.worldTransform * glm::vec4(mesh.aabb[0], 1.0),
            transform.worldTransform * glm::vec4(mesh.aabb[1], 1.0)
        };

        Math::Frustrum frustrum;
        frustrum.update(mapProjection * mapView, true);

        // if the frustrum can't see the mesh's OBB we cull it
        if (!frustrum.vsAABB(worldAABB[0], worldAABB[1])) {
            continue;
        }

        shader.getUniform("model") = transform.worldTransform;

        // determine if we use the original mesh vertices or GPU skinned vertices
        if (scene.has<ecs::MeshAnimationComponent>(entity)) {
            scene.get<ecs::MeshAnimationComponent>(entity).skinnedVertexBuffer.bind();
        }
        else {
            mesh.vertexBuffer.bind();
        }
        mesh.indexBuffer.bind();
        glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indices.size(), GL_UNSIGNED_INT, nullptr);

    }

    glCullFace(GL_BACK);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

OmniShadowMap::OmniShadowMap(uint32_t width, uint32_t height) {
    settings.width = width, settings.height = height;

    std::vector<Shader::Stage> omniShadowmapStages;
    omniShadowmapStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\depthCube.vert");
    omniShadowmapStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\depthCube.frag");
    shader.reload(omniShadowmapStages.data(), omniShadowmapStages.size());

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &result);
    glTextureStorage3D(result, 1, GL_DEPTH_COMPONENT32F, settings.width, settings.height, 6);

    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(result, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(result, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(result, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

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
    glBindFramebuffer(GL_FRAMEBUFFER, depthCubeFramebuffer);
    glCullFace(GL_BACK);

    shader.bind();
    shader.getUniform("farPlane") = settings.farPlane;
    for (uint32_t i = 0; i < 6; i++) {
        glNamedFramebufferTexture(depthCubeFramebuffer, GL_COLOR_ATTACHMENT0 + i, result, i);
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
            glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indices.size(), GL_UNSIGNED_INT, nullptr);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

GeometryBuffer::GeometryBuffer(Viewport& viewport) {
    std::vector<Shader::Stage> gbufferStages;
    gbufferStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\gbuffer.vert");
    gbufferStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\gbuffer.frag");
    shader.reload(gbufferStages.data(), gbufferStages.size());
    hotloader.watch(&shader, gbufferStages.data(), gbufferStages.size());

    createResources(viewport);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

GeometryBuffer::~GeometryBuffer() {
    deleteResources();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void GeometryBuffer::execute(entt::registry& scene, Viewport& viewport) {
    hotloader.changed();

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

            ecs::MaterialComponent* material = nullptr;
            if (scene.valid(mesh.material)) {
                material = scene.try_get<ecs::MaterialComponent>(mesh.material);
            }

        if (material) {
            if (material->albedo)  glBindTextureUnit(0, material->albedo);
            else glBindTextureUnit(0, ecs::MaterialComponent::Default.albedo);

            if (material->normals) glBindTextureUnit(3, material->normals);
            else glBindTextureUnit(3, ecs::MaterialComponent::Default.normals);

            if (material->metalrough) glBindTextureUnit(4, material->metalrough);
            else glBindTextureUnit(4, ecs::MaterialComponent::Default.metalrough);

            shader.getUniform("colour") = material->baseColour;

        } else {
            glBindTextureUnit(0, ecs::MaterialComponent::Default.albedo);
            glBindTextureUnit(3, ecs::MaterialComponent::Default.normals);
            glBindTextureUnit(4, ecs::MaterialComponent::Default.metalrough);
            shader.getUniform("colour") = ecs::MaterialComponent::Default.baseColour;
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
        glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indices.size(), GL_UNSIGNED_INT, nullptr);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t GeometryBuffer::readEntity(GLint x, GLint y) {
    float pixel;
    glGetTextureSubImage(entityTexture, 0, x, y,
        0, 1, 1, 1, GL_RED, GL_FLOAT, sizeof(float), &pixel);
    return static_cast<uint32_t>(pixel);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void GeometryBuffer::createResources(Viewport& viewport) {
    glCreateTextures(GL_TEXTURE_2D, 1, &albedoTexture);
    glTextureStorage2D(albedoTexture, 1, GL_RGBA16F, viewport.size.x, viewport.size.y);
    glTextureParameteri(albedoTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(albedoTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateTextures(GL_TEXTURE_2D, 1, &normalTexture);
    glTextureStorage2D(normalTexture, 1, GL_RGBA16F, viewport.size.x, viewport.size.y);
    glTextureParameteri(albedoTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(albedoTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateTextures(GL_TEXTURE_2D, 1, &materialTexture);
    glTextureStorage2D(materialTexture, 1, GL_RG16F, viewport.size.x, viewport.size.y);
    glTextureParameteri(materialTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(materialTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateTextures(GL_TEXTURE_2D, 1, &entityTexture);
    glTextureStorage2D(entityTexture, 1, GL_R32F, viewport.size.x, viewport.size.y);
    glTextureParameteri(entityTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(entityTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateTextures(GL_TEXTURE_2D, 1, &depthTexture);
    glTextureStorage2D(depthTexture, 1, GL_DEPTH_COMPONENT32F, viewport.size.x, viewport.size.y);
    glTextureParameteri(depthTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(depthTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateFramebuffers(1, &GBuffer);
    glNamedFramebufferTexture(GBuffer, GL_COLOR_ATTACHMENT0, normalTexture, 0);
    glNamedFramebufferTexture(GBuffer, GL_COLOR_ATTACHMENT1, albedoTexture, 0);
    glNamedFramebufferTexture(GBuffer, GL_COLOR_ATTACHMENT2, materialTexture, 0);
    glNamedFramebufferTexture(GBuffer, GL_COLOR_ATTACHMENT3, entityTexture, 0);

    std::array<GLenum, 4> colorAttachments = { 
        GL_COLOR_ATTACHMENT0, 
        GL_COLOR_ATTACHMENT1, 
        GL_COLOR_ATTACHMENT2, 
        GL_COLOR_ATTACHMENT3,
    };

    glNamedFramebufferDrawBuffers(GBuffer, static_cast<GLsizei>(colorAttachments.size()), colorAttachments.data());
    glNamedFramebufferTexture(GBuffer, GL_DEPTH_ATTACHMENT, depthTexture, 0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void GeometryBuffer::deleteResources() {
    std::array<unsigned int, 5> textures = { 
        albedoTexture, 
        normalTexture, 
        materialTexture, 
        depthTexture, 
        entityTexture 
    };

    glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
    glDeleteFramebuffers(1, &GBuffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

ScreenSpaceAmbientOcclusion::~ScreenSpaceAmbientOcclusion() {
    deleteResources();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////////////////////////////

void ScreenSpaceAmbientOcclusion::execute(Viewport& viewport, GeometryBuffer* geometryPass) {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
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

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // now blur the SSAO result
    glBindFramebuffer(GL_FRAMEBUFFER, blurFramebuffer);
    glBindTextureUnit(0, preblurResult);
    blurShader.bind();

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////////////////////////////

void ScreenSpaceAmbientOcclusion::deleteResources() {
    std::array<GLuint, 3> textures = { noiseTexture, result, preblurResult };
    glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());

    glDeleteFramebuffers(1, &framebuffer);
    glDeleteFramebuffers(1, &blurFramebuffer);
}

//////////////////////////////////////////////////////////////////////////////////

DeferredLighting::~DeferredLighting() {
    deleteResources();
}

//////////////////////////////////////////////////////////////////////////////////

DeferredLighting::DeferredLighting(Viewport& viewport) {
    // load shaders from disk

    Shader::Stage vertex(Shader::Type::VERTEX, "shaders\\OpenGL\\quad.vert");
    Shader::Stage frag(Shader::Type::FRAG, "shaders\\OpenGL\\main.frag");
    std::array<Shader::Stage, 2> modelStages = { vertex, frag };
    shader.reload(modelStages.data(), modelStages.size());

    hotloader.watch(&shader, modelStages.data(), modelStages.size());

    // init resources
    createResources(viewport);

    // init uniform buffer
    uniformBuffer.setSize(sizeof(uniforms));
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void DeferredLighting::execute(entt::registry& sscene, Viewport& viewport, ShadowMap* shadowMap, OmniShadowMap* omniShadowMap,
                                GeometryBuffer* GBuffer, ScreenSpaceAmbientOcclusion* ambientOcclusion, Voxelization* voxels, unsigned int irradianceMap) {
    hotloader.changed();

    // update the uniform buffer
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
        } else {
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

    // update uniforms GPU side
    uniformBuffer.update(&uniforms, sizeof(uniforms));

    // bind the main framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glClear(GL_COLOR_BUFFER_BIT);

    // set uniforms
    shader.bind();
    shader.getUniform("bloomThreshold") = settings.bloomThreshold;

    shader.getUniform("pointLightCount") = static_cast<uint32_t>(sscene.view<ecs::PointLightComponent>().size());
    shader.getUniform("directionalLightCount") = static_cast<uint32_t>(sscene.view<ecs::DirectionalLightComponent>().size());

    shader.getUniform("voxelsWorldSize") = voxels->worldSize;

    // TODO: why on earth does this need to be here?? If I just do:
    // inverse(projection * view) in the shader we get shadow flickering.
    // Probably something to do with how the uniform buffer is updated?
    shader.getUniform("invViewProjection") = glm::inverse(uniforms.projection * uniforms.view);

    // bind textures to shader binding slots
    glBindTextureUnit(0, shadowMap->result);
    
    if (omniShadowMap) {
        glBindTextureUnit(1, omniShadowMap->result);
    }

    glBindTextureUnit(3, GBuffer->albedoTexture);
    glBindTextureUnit(4, GBuffer->normalTexture);

    if (ambientOcclusion) {
        glBindTextureUnit(5, ambientOcclusion->result);
    }

    glBindTextureUnit(6, voxels->result);
    glBindTextureUnit(7, GBuffer->materialTexture);
    glBindTextureUnit(8, GBuffer->depthTexture);
    glBindTextureUnit(9, irradianceMap);



    // update uniform buffer GPU side
    uniformBuffer.bind(0);

    // perform lighting pass and unbind
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void DeferredLighting::createResources(Viewport& viewport) {
    // init render targets
    glCreateTextures(GL_TEXTURE_2D, 1, &result);
    glTextureStorage2D(result, 1, GL_RGBA16F, viewport.size.x, viewport.size.y);
    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glCreateTextures(GL_TEXTURE_2D, 1, &bloomHighlights);
    glTextureStorage2D(bloomHighlights, 1, GL_RGBA16F, viewport.size.x,  viewport.size.y);
    glTextureParameteri(bloomHighlights, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(bloomHighlights, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(bloomHighlights, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(bloomHighlights, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(bloomHighlights, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glCreateFramebuffers(1, &framebuffer);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT0, result, 0);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT1, bloomHighlights, 0);

    auto colorAttachments = std::array<GLenum, 4> { 
        GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1    
    };

    glNamedFramebufferDrawBuffers(framebuffer, static_cast<GLsizei>(colorAttachments.size()), colorAttachments.data());
}

void DeferredLighting::deleteResources() {
    glDeleteTextures(1, &result);
    glDeleteTextures(1, &bloomHighlights);
    glDeleteFramebuffers(1, &framebuffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

Bloom::~Bloom() {
    deleteResources();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

Bloom::Bloom(Viewport& viewport) {
    // load shaders from disk
    Shader::Stage blurStages[2] = {
        Shader::Stage(Shader::Type::VERTEX, "shaders\\OpenGL\\quad.vert"),
        Shader::Stage(Shader::Type::FRAG, "shaders\\OpenGL\\gaussian.frag")
    };

    blurShader.reload(blurStages, 2);


    Shader::Stage downsampleStages[2] = {
        Shader::Stage(Shader::Type::VERTEX, "shaders\\OpenGL\\quad.vert"),
        Shader::Stage(Shader::Type::FRAG, "shaders\\OpenGL\\downsample.frag")
    };

    downsampleShader.reload(downsampleStages, 2);

    createResources(viewport);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Bloom::execute(Viewport& viewport, unsigned int highlights) {
    if (viewport.size.x < 16.0f || viewport.size.y < 16.0f) {
        return;
    }

    auto quarter = glm::ivec2(viewport.size.x / 4, viewport.size.y / 4);
    
    // downsample to quarter screen res
    glBindFramebuffer(GL_FRAMEBUFFER, bloomFramebuffer);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(0, 0, quarter.x, quarter.y);

    downsampleShader.bind();
    glBindTextureUnit(0, highlights);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    blurShader.bind();
    
    // horizontally blur 1/4th bloom to blur texture
    glBindFramebuffer(GL_FRAMEBUFFER, blurFramebuffer);
    glClear(GL_COLOR_BUFFER_BIT);

    blurShader.getUniform("direction") = glm::vec2(1, 0);
    glBindTextureUnit(0, bloomTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // vertically blur the blur to bloom texture
    glBindFramebuffer(GL_FRAMEBUFFER, bloomFramebuffer);
    glClear(GL_COLOR_BUFFER_BIT);

    blurShader.getUniform("direction") = glm::vec2(0, 1);
    glBindTextureUnit(0, blurTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glViewport(0, 0, viewport.size.x, viewport.size.y);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Bloom::createResources(Viewport& viewport) {
    auto quarterRes = glm::ivec2(viewport.size.x / 4, viewport.size.y / 4);

    glCreateTextures(GL_TEXTURE_2D, 1, &bloomTexture);
    glTextureStorage2D(bloomTexture, 1, GL_RGBA16F, quarterRes.x, quarterRes.y);
    glTextureParameteri(bloomTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(bloomTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(bloomTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(bloomTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(bloomTexture, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glCreateTextures(GL_TEXTURE_2D, 1, &blurTexture);
    glTextureStorage2D(blurTexture, 1, GL_RGBA16F, quarterRes.x, quarterRes.y);
    glTextureParameteri(blurTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(blurTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(blurTexture, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTextureParameteri(blurTexture, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTextureParameteri(blurTexture, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT);

    glCreateFramebuffers(1, &bloomFramebuffer);
    glNamedFramebufferTexture(bloomFramebuffer, GL_COLOR_ATTACHMENT0, bloomTexture, 0);
    glNamedFramebufferDrawBuffer(bloomFramebuffer, GL_COLOR_ATTACHMENT0);

    glCreateFramebuffers(1, &blurFramebuffer);
    glNamedFramebufferTexture(blurFramebuffer, GL_COLOR_ATTACHMENT0, blurTexture, 0);
    glNamedFramebufferDrawBuffer(blurFramebuffer, GL_COLOR_ATTACHMENT0);

}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Bloom::deleteResources() {
    glDeleteFramebuffers(1, &bloomFramebuffer);
    glDeleteFramebuffers(1, &blurFramebuffer);
    glDeleteTextures(1, &bloomTexture);
    glDeleteTextures(1, &blurTexture);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

Tonemapping::~Tonemapping() {
    deleteResources();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

Tonemapping::Tonemapping(Viewport& viewport) {
    // load shaders from disk
    std::vector<Shader::Stage> tonemapStages;
    tonemapStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\quad.vert");
    tonemapStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\HDRuncharted.frag");
    shader.reload(tonemapStages.data(), tonemapStages.size());

    // init render targets
    createResources(viewport);

    // init uniform buffer
    uniformBuffer.setSize(sizeof(settings));
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Tonemapping::execute(unsigned int scene, unsigned int bloom) {
    // bind and clear render target
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // bind shader and input texture
    shader.bind();
    glBindTextureUnit(0, scene);
    glBindTextureUnit(1, bloom);

    // update uniform buffer GPU side
    uniformBuffer.update(&settings, sizeof(settings));
    uniformBuffer.bind(0);

    // render fullscreen quad to perform tonemapping
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////////////////////////////

void Tonemapping::deleteResources() {
    glDeleteTextures(1, &result);
    glDeleteFramebuffers(1, &framebuffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

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

    glCreateTextures(GL_TEXTURE_3D, 1, &result);
    glTextureStorage3D(result, static_cast<GLsizei>(std::log2(size)), GL_RGBA8, size, size, size);
    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateTextureMipmap(result);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Voxelization::computeMipmaps(unsigned int texture) {
    int level = 0, texSize = size;
    while (texSize >= 1.0f) {
        texSize = static_cast<int>(texSize * 0.5f);
        mipmapShader.bind();
        glBindImageTexture(0, texture, level, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA8);
        glBindImageTexture(1, texture, level + 1, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
        // TODO: query local work group size at startup
        glDispatchCompute(static_cast<GLuint>(size / 64), texSize, texSize);
        mipmapShader.unbind();
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        level++;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Voxelization::correctOpacity(unsigned int texture) {
    opacityFixShader.bind();
    glBindImageTexture(0, texture, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
    // local work group size is 64
    glDispatchCompute(static_cast<GLuint>(size / 64), size, size);
    opacityFixShader.unbind();
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Voxelization::execute(entt::registry& scene, Viewport& viewport, ShadowMap* shadowmap) {
    hotloader.changed();

    // left, right, bottom, top, zNear, zFar
    auto projectionMatrix = glm::ortho(-worldSize * 0.5f, worldSize * 0.5f, -worldSize * 0.5f, worldSize * 0.5f, worldSize * 0.5f, worldSize * 1.5f);
    px = projectionMatrix * glm::lookAt(glm::vec3(worldSize, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    py = projectionMatrix * glm::lookAt(glm::vec3(0, worldSize, 0), glm::vec3(0, 0, 0), glm::vec3(0, 0, -1));
    pz = projectionMatrix * glm::lookAt(glm::vec3(0, 0, worldSize), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

    // clear the entire voxel texture
    constexpr auto clearColour = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    for (uint32_t level = 0; level < std::log2(size); level++) {
        glClearTexImage(result, level, GL_RGBA, GL_FLOAT, glm::value_ptr(clearColour));
    }

    // set GL state
    glViewport(0, 0, size, size);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // bind shader and level 0 of the voxel volume
    shader.bind();
    glBindImageTexture(1, result, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32UI);
    glBindTextureUnit(2, shadowmap->result);

    shader.getUniform("lightViewProjection") = shadowmap->uniforms.cameraMatrix;

    auto view = scene.view<ecs::MeshComponent, ecs::TransformComponent>();

    for (auto entity : view) {
        auto& [mesh, transform] = view.get<ecs::MeshComponent, ecs::TransformComponent>(entity);

        ecs::MaterialComponent* material = nullptr;
        if (scene.valid(mesh.material)) {
            material = scene.try_get<ecs::MaterialComponent>(mesh.material);
        }

        shader.getUniform("model") = transform.worldTransform;
        shader.getUniform("px") = px;
        shader.getUniform("py") = py;
        shader.getUniform("pz") = pz;


        if (material) {
            if (material->albedo)  glBindTextureUnit(0, material->albedo);
            else glBindTextureUnit(0, ecs::MaterialComponent::Default.albedo);
            shader.getUniform("colour") = material->baseColour;
        } else {
            glBindTextureUnit(0, ecs::MaterialComponent::Default.albedo);
            shader.getUniform("colour") = ecs::MaterialComponent::Default.baseColour;
        }

        // determine if we use the original mesh vertices or GPU skinned vertices
        if (scene.has<ecs::MeshAnimationComponent>(entity)) {
            scene.get<ecs::MeshAnimationComponent>(entity).skinnedVertexBuffer.bind();
        }
        else {
            mesh.vertexBuffer.bind();
        }

        mesh.indexBuffer.bind();
        glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indices.size(), GL_UNSIGNED_INT, nullptr);
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

//////////////////////////////////////////////////////////////////////////////////////////////////

VoxelizationDebug::~VoxelizationDebug() {
    deleteResources();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

VoxelizationDebug::VoxelizationDebug(Viewport& viewport) {
    std::vector<Shader::Stage> voxelDebugStages;
    voxelDebugStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\voxelDebug.vert");
    voxelDebugStages.emplace_back(Shader::Type::GEO, "shaders\\OpenGL\\voxelDebug.geom");
    voxelDebugStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\voxelDebug.frag");
    shader.reload(voxelDebugStages.data(), voxelDebugStages.size());

    // init resources
    createResources(viewport);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

VoxelizationDebug::VoxelizationDebug(Viewport& viewport, uint32_t voxelTextureSize) {
    std::vector<Shader::Stage> voxelDebugStages;
    voxelDebugStages.emplace_back(Shader::Type::VERTEX, "shaders\\OpenGL\\voxelDebugFast.vert");
    voxelDebugStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\voxelDebugFast.frag");
    shader.reload(voxelDebugStages.data(), voxelDebugStages.size());

    // init resources
    createResources(viewport);

    const bool CUBE_BACKFACE_OPTIMIZATION = true;

    constexpr size_t NUM_CUBE_INDICES = CUBE_BACKFACE_OPTIMIZATION ? 3 * 3 * 2 : 3 * 6 * 2;
    constexpr size_t NUM_CUBE_VERTICES = 8;

    constexpr std::array<uint32_t, 36> cubeIndices = {
        0, 2, 1, 2, 3, 1,
        5, 4, 1, 1, 4, 0,
        0, 4, 6, 0, 6, 2,
        6, 5, 7, 6, 4, 5,
        2, 6, 3, 6, 7, 3,
        7, 1, 3, 7, 5, 1
    };

    const size_t numIndices = static_cast<size_t>(std::pow(voxelTextureSize, 3u) * NUM_CUBE_INDICES);

    std::vector<uint32_t> indexBufferData(numIndices);

    const auto chunks = { 1, 2, 3, 4, 5, 6, 7, 8 };
    const size_t CHUNK_SIZE = numIndices / chunks.size();


    std::for_each(std::execution::par_unseq, chunks.begin(), chunks.end(), [&](const int chunk) {
        const size_t start = (chunk - 1) * CHUNK_SIZE;
        const size_t end = chunk * CHUNK_SIZE;

        for (size_t i = start; i < end; i++) {
            auto cube = i / NUM_CUBE_INDICES;
            auto cube_local = i % NUM_CUBE_INDICES;
            indexBufferData[i] = static_cast<uint32_t>(cubeIndices[cube_local] + cube * NUM_CUBE_VERTICES);
        }
    });

    indexBuffer.loadIndices(indexBufferData.data(), indexBufferData.size());
    indexCount = static_cast<uint32_t>(indexBufferData.size());
}

void VoxelizationDebug::execute(Viewport& viewport, unsigned int input, Voxelization* voxels) {
    // bind the input framebuffer, we draw the debug vertices on top
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
    glNamedFramebufferTexture(frameBuffer, GL_COLOR_ATTACHMENT0, input, 0);
    glNamedFramebufferDrawBuffer(frameBuffer, GL_COLOR_ATTACHMENT0);
    glClear(GL_DEPTH_BUFFER_BIT);

    float voxelSize = voxels->worldSize / voxels->size;
    glm::mat4 modelMatrix = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(voxelSize)), glm::vec3(0, 0, 0));

    // bind shader and set uniforms
    shader.bind();
    shader.getUniform("p") = viewport.getCamera().getProjection();
    shader.getUniform("mv") = viewport.getCamera().getView() * modelMatrix;

    glBindTextureUnit(0, voxels->result);

    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(std::pow(voxels->size, 3)));

    // unbind framebuffers
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelizationDebug::execute2(Viewport& viewport, unsigned int input, Voxelization* voxels) {
    // bind the input framebuffer, we draw the debug vertices on top
    glDisable(GL_CULL_FACE);    

    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
    glNamedFramebufferTexture(frameBuffer, GL_COLOR_ATTACHMENT0, input, 0);
    glNamedFramebufferDrawBuffer(frameBuffer, GL_COLOR_ATTACHMENT0);
    glClear(GL_DEPTH_BUFFER_BIT);

    float voxelSize = voxels->worldSize / voxels->size;
    glm::mat4 modelMatrix = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(voxelSize)), glm::vec3(0, 0, 0));

    // bind shader and set uniforms
    shader.bind();
    shader.getUniform("p") = viewport.getCamera().getProjection();
    shader.getUniform("mv") = viewport.getCamera().getView() * modelMatrix;
    shader.getUniform("cameraPosition") = viewport.getCamera().getPosition();

    glBindTextureUnit(0, voxels->result);

    indexBuffer.bind();
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);

    // unbind framebuffers
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glEnable(GL_CULL_FACE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelizationDebug::createResources(Viewport& viewport) {
    glCreateRenderbuffers(1, &renderBuffer);
    glNamedRenderbufferStorage(renderBuffer, GL_DEPTH32F_STENCIL8, viewport.size.x, viewport.size.y);

    glCreateFramebuffers(1, &frameBuffer);
    glNamedFramebufferRenderbuffer(frameBuffer, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderBuffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelizationDebug::deleteResources() {
    glDeleteRenderbuffers(1, &renderBuffer);
    glDeleteFramebuffers(1, &frameBuffer);
}

//////////////////////////////////////////////////////////////////////////////////

BoundingBoxDebug::~BoundingBoxDebug() {
    deleteResources();
}

//////////////////////////////////////////////////////////////////////////////////

BoundingBoxDebug::BoundingBoxDebug(Viewport& viewport) {
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

//////////////////////////////////////////////////////////////////////////////////////////////////

void BoundingBoxDebug::execute(entt::registry& scene, Viewport& viewport, unsigned int texture, unsigned int renderBuffer, entt::entity active) {
    if (active == entt::null) return;
    if (!scene.has<ecs::MeshComponent>(active) || !scene.has<ecs::TransformComponent>(active)) {
        return;
    }

    auto& mesh = scene.get<ecs::MeshComponent>(active);
    auto& transform = scene.get<ecs::TransformComponent>(active);

    glEnable(GL_LINE_SMOOTH);

    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
    glNamedFramebufferTexture(frameBuffer, GL_COLOR_ATTACHMENT0, texture, 0);
    glNamedFramebufferTexture(frameBuffer, GL_DEPTH_ATTACHMENT, renderBuffer, 0);
    glNamedFramebufferDrawBuffer(frameBuffer, GL_COLOR_ATTACHMENT0);

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

    glDrawElements(GL_LINES, (GLsizei)indexBuffer.count, GL_UNSIGNED_INT, nullptr);

    glDisable(GL_LINE_SMOOTH);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void BoundingBoxDebug::createResources(Viewport& viewport) {
    glCreateTextures(GL_TEXTURE_2D, 1, &result);
    glTextureStorage2D(result, 1, GL_RGBA32F, viewport.size.x, viewport.size.y);
    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateFramebuffers(1, &frameBuffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void BoundingBoxDebug::deleteResources() {
    glDeleteTextures(1, &result);
    glDeleteFramebuffers(1, &frameBuffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

Skinning::Skinning() {
    std::vector<Shader::Stage> stages;
    stages.emplace_back(Shader::Type::COMPUTE, "shaders\\OpenGL\\skinning.comp");
    computeShader.reload(stages.data(), stages.size());
    hotloader.watch(&computeShader, stages.data(), stages.size());
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Skinning::execute(ecs::MeshComponent& mesh, ecs::MeshAnimationComponent& anim) {

    glNamedBufferData(anim.boneTransformsBuffer, anim.boneTransforms.size() * sizeof(glm::mat4), anim.boneTransforms.data(), GL_DYNAMIC_DRAW);

    computeShader.bind();

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, anim.boneIndexBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, anim.boneWeightBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, mesh.vertexBuffer.id);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, anim.skinnedVertexBuffer.id);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, anim.boneTransformsBuffer);

    glDispatchCompute(static_cast<GLuint>(mesh.positions.size()), 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

inline double random_double() {
    static std::uniform_real_distribution<double> distribution(0.0, 1.0);
    static std::mt19937 generator;
    return distribution(generator);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

inline double random_double(double min, double max) {
    static std::uniform_real_distribution<double> distribution(min, max);
    static std::mt19937 generator;
    return distribution(generator);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

inline glm::vec3 random_color() {
    return glm::vec3(random_double(), random_double(), random_double());
}

//////////////////////////////////////////////////////////////////////////////////////////////////

inline glm::vec3 random_color(double min, double max) {
    return glm::vec3(random_double(min, max), random_double(min, max), random_double(min, max));
}

//////////////////////////////////////////////////////////////////////////////////////////////////

RayCompute::RayCompute(Viewport& viewport) {
    std::vector<Shader::Stage> stages;
    stages.emplace_back(Shader::Type::COMPUTE, "shaders\\OpenGL\\ray.comp");
    shader.reload(stages.data(), stages.size());
    hotloader.watch(&shader, stages.data(), stages.size());

    spheres.push_back(Sphere{ glm::vec3(0, -1000, 0), glm::vec3(0.5, 0.5, 0.5), 1.0f, 0.0f, 1000.0f });

    int count = 3;
    for (int a = -count; a < count; a++) {
        for (int b = -count; b < count; b++) {
            auto choose_mat = random_double();
            glm::vec3 center(a + 0.9 * random_double(), 0.2, b + 0.9 * random_double());

            if ((center - glm::vec3(4, 0.2, 0)).length() > 0.9) {
                Sphere sphere;

                if (choose_mat < 0.8) {
                    // diffuse
                    sphere.colour = random_color() * random_color();
                    sphere.metalness = 0.0f;
                    sphere.radius = 0.2f;
                    sphere.origin = center;
                    spheres.push_back(sphere);
                }
                else if (choose_mat < 0.95) {
                    // metal
                    sphere.colour = random_color(0.5, 1);
                    sphere.roughness = static_cast<float>(random_double(0, 0.5));
                    sphere.metalness = 1.0f;
                    sphere.radius = 0.2f;
                    sphere.origin = center;
                    spheres.push_back(sphere);
                }
            }
        }
    }

    spheres.push_back(Sphere{ glm::vec3(-4, 1, 0), glm::vec3(0.4, 0.2, 0.1), 1.0f, 0.0f, 1.0f });
    spheres.push_back(Sphere{ glm::vec3(4, 1, 0), glm::vec3(0.7, 0.6, 0.5), 0.0f, 1.0f, 1.0f });

    createResources(viewport);
    auto clearColour = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    glClearTexImage(result, 0, GL_RGBA, GL_FLOAT, glm::value_ptr(clearColour));

    rayTimer.start();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

RayCompute::~RayCompute() {
    rayTimer.stop();
    deleteResources();

}

//////////////////////////////////////////////////////////////////////////////////////////////////

void RayCompute::execute(Viewport& viewport, bool update) {
    // if the shader changed or we moved the camera we clear the result
    if (!update) {
        glDeleteBuffers(1, &sphereBuffer);
        glCreateBuffers(1, &sphereBuffer);
        glNamedBufferStorage(sphereBuffer, spheres.size() * sizeof(Sphere), spheres.data(), GL_DYNAMIC_STORAGE_BIT);
    }

    shader.bind();
    glBindImageTexture(0, result, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
    glBindImageTexture(1, finalResult, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, sphereBuffer);

    shader.getUniform("iTime") = static_cast<float>(rayTimer.elapsedMs() / 1000);
    shader.getUniform("position") = viewport.getCamera().getPosition();
    shader.getUniform("projection") = viewport.getCamera().getProjection();
    shader.getUniform("view") = viewport.getCamera().getView(); 
    shader.getUniform("doUpdate") = update;

    const GLuint numberOfSpheres = static_cast<GLuint>(spheres.size());
    shader.getUniform("sphereCount") = numberOfSpheres;

    glDispatchCompute(viewport.size.x / 16 , viewport.size.y / 16, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void RayCompute::createResources(Viewport& viewport) {
    glCreateTextures(GL_TEXTURE_2D, 1, &result);
    glTextureStorage2D(result, 1, GL_RGBA32F, viewport.size.x, viewport.size.y);
    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateTextures(GL_TEXTURE_2D, 1, &finalResult);
    glTextureStorage2D(finalResult, 1, GL_RGBA32F, viewport.size.x, viewport.size.y);
    glTextureParameteri(finalResult, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(finalResult, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateBuffers(1, &sphereBuffer);
    glNamedBufferStorage(sphereBuffer, spheres.size() * sizeof(Sphere), spheres.data(), GL_DYNAMIC_STORAGE_BIT);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void RayCompute::deleteResources() {
    glDeleteTextures(1, &result);
    glDeleteTextures(1, &finalResult);
    glDeleteBuffers(1, &sphereBuffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

Skydome::Skydome(Viewport& viewport) {
    Shader::Stage stages[2] = {
        Shader::Stage(Shader::Type::VERTEX, "shaders\\OpenGL\\skydome.vert"),
        Shader::Stage(Shader::Type::FRAG, "shaders\\OpenGL\\skydome.frag")
    };

    shader.reload(stages, 2);
    hotloader.watch(&shader, stages, 2);

    auto importer = std::make_unique<Assimp::Importer>();
    auto scene = importer->ReadFile("resources/models/wtfsphere.obj",
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_SortByPType |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenUVCoords |
        aiProcess_ValidateDataStructure);

    if (!scene) std::cout << importer->GetErrorString() << '\n';

    assert(scene->HasMeshes());
    AssimpImporter::convertMesh(sphere, scene->mMeshes[0]);
    sphere.uploadVertices();
    sphere.uploadIndices();

    createResources(viewport);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

Skydome::~Skydome() {

    deleteResources();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Skydome::execute(Viewport& viewport, unsigned int texture, unsigned int depth) {
    hotloader.changed();

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT0, texture, 0);
    glNamedFramebufferDrawBuffer(framebuffer, GL_COLOR_ATTACHMENT0);
    glNamedFramebufferTexture(framebuffer, GL_DEPTH_ATTACHMENT, depth, 0);

    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);

    shader.bind();
    shader.getUniform("projection") = viewport.getCamera().getProjection();
    shader.getUniform("view") = glm::mat4(glm::mat3(viewport.getCamera().getView()));
    shader.getUniform("mid_color") = settings.mid_color;
    shader.getUniform("top_color") = settings.top_color;

    sphere.vertexBuffer.bind();
    sphere.indexBuffer.bind();
    glDrawElements(GL_TRIANGLES, (GLsizei)sphere.indices.size(), GL_UNSIGNED_INT, nullptr);

    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Skydome::createResources(Viewport& viewport) {
    glCreateFramebuffers(1, &framebuffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Skydome::deleteResources() {
    glDeleteFramebuffers(1, &framebuffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

WorldIcons::WorldIcons(Viewport& viewport) {
    Shader::Stage stages[2] = {
        Shader::Stage(Shader::Type::VERTEX, "shaders\\OpenGL\\billboard.vert"),
        Shader::Stage(Shader::Type::FRAG, "shaders\\OpenGL\\billboard.frag")
    };

    shader.reload(stages, 2);

    int w, h, ch;
    stbi_set_flip_vertically_on_load(true);
    auto img = stbi_load("resources/light.png", &w, &h, &ch, 4);

    glCreateTextures(GL_TEXTURE_2D, 1, &lightTexture);
    glTextureStorage2D(lightTexture, 1, GL_RGBA8, w, h);
    glTextureSubImage2D(lightTexture, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glTextureParameteri(lightTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(lightTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(lightTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(lightTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(lightTexture, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glGenerateTextureMipmap(lightTexture);
    createResources(viewport);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void WorldIcons::createResources(Viewport& viewport) {
    glCreateFramebuffers(1, &framebuffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void WorldIcons::destroyResources() {
    glDeleteRenderbuffers(1, &framebuffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void WorldIcons::execute(entt::registry& scene, Viewport& viewport, unsigned int screenTexture, unsigned int entityTexture) {
    glDisable(GL_CULL_FACE);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT0, screenTexture, 0);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT1, entityTexture, 0);

    GLenum attachments[2] = { GL_COLOR_ATTACHMENT0 , GL_COLOR_ATTACHMENT1 };
    glNamedFramebufferDrawBuffers(framebuffer, 2, attachments);

    glBindTextureUnit(0, lightTexture);

    shader.bind();

    auto vp = viewport.getCamera().getProjection() * viewport.getCamera().getView();

    auto view = scene.view<ecs::DirectionalLightComponent, ecs::TransformComponent>();
    for (auto entity : view) {
        auto& light = view.get<ecs::DirectionalLightComponent>(entity);
        auto& transform = view.get<ecs::TransformComponent>(entity);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, transform.position);

        glm::vec3 V;
        V.x = viewport.getCamera().getPosition().x - transform.position.x;
        V.y = 0;
        V.z = viewport.getCamera().getPosition().z - transform.position.z;

        auto Vnorm = glm::normalize(V);

        glm::vec3 lookAt = glm::vec3(0.0f, 0.0f, 1.0f);

        auto upAux = glm::cross(lookAt, Vnorm);
        auto cosTheta = glm::dot(lookAt, Vnorm);

        model = glm::rotate(model, acos(cosTheta), upAux);

        model = glm::scale(model, glm::vec3(glm::length(V) * 0.05f));

        shader.getUniform("mvp") = vp * model;

        shader.getUniform("entity") = entt::to_integral(entity);
        shader.getUniform("world_position") = transform.position;

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glEnable(GL_CULL_FACE);
}

HDRSky::HDRSky() {
    glShader::Stage equiStages[2] = {
        glShader::Stage(Shader::Type::VERTEX, "shaders\\OpenGL\\equiToCubemap.vert"),
        glShader::Stage(Shader::Type::FRAG, "shaders\\OpenGL\\equiToCubemap.frag")
    };
    equiToCubemapShader.reload(equiStages, 2);

    std::vector skyboxStages {
        glShader::Stage(Shader::Type::VERTEX, "shaders\\OpenGL\\skybox.vert"),
        glShader::Stage(Shader::Type::FRAG, "shaders\\OpenGL\\skybox.frag")
    };



    skyboxShader.reload(skyboxStages.data(), skyboxStages.size());

    glShader::Stage convoluteStages[2] = {
        glShader::Stage(Shader::Type::VERTEX, "shaders\\OpenGL\\skybox.vert"),
        glShader::Stage(Shader::Type::FRAG, "shaders\\OpenGL\\convolute.frag")
    };
    convoluteShader.reload(convoluteStages, 2);

    std::vector prefilterStages {
        glShader::Stage(Shader::Type::VERTEX, "shaders\\OpenGL\\skybox.vert"),
        glShader::Stage(Shader::Type::FRAG, "shaders\\OpenGL\\prefilter.frag")
    };

    prefilterShader.reload(prefilterStages.data(), prefilterStages.size());

    for (const auto& v : unitCubeVertices) {
        glm::vec3 glPos = v.pos * glm::vec3(2.0) - glm::vec3(1.0);
        unitCube.positions.push_back(glPos);
    }

    for (const auto& idx : cubeIndices) {
        unitCube.indices.push_back(idx.p1);
        unitCube.indices.push_back(idx.p2);
        unitCube.indices.push_back(idx.p3);
    }

    unitCube.uploadVertices();
    unitCube.uploadIndices();

    glCreateFramebuffers(1, &captureFramebuffer);
    glCreateFramebuffers(1, &skyboxFramebuffer);
    glCreateFramebuffers(1, &prefilterFramebuffer);
    glCreateRenderbuffers(1, &captureRenderbuffer);
    glCreateRenderbuffers(1, &convRenderbuffer);

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &environmentMap);
    glTextureStorage2D(environmentMap, 1, GL_RGB16F, 512, 512);
    glTextureParameteri(environmentMap, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(environmentMap, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(environmentMap, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTextureParameteri(environmentMap, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(environmentMap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &irradianceMap);
    glTextureStorage2D(irradianceMap, 1, GL_RGB16F, 32, 32);
    glTextureParameteri(irradianceMap, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(irradianceMap, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(irradianceMap, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTextureParameteri(irradianceMap, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(irradianceMap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glNamedRenderbufferStorage(captureRenderbuffer, GL_DEPTH_COMPONENT24, 512, 512);

    glNamedRenderbufferStorage(convRenderbuffer, GL_DEPTH_COMPONENT24, 32, 32);

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &prefilterMap);
    glTextureStorage2D(prefilterMap, 5, GL_RGB16F, 128, 128);
    glTextureParameteri(prefilterMap, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(prefilterMap, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(prefilterMap, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTextureParameteri(prefilterMap, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(prefilterMap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateTextureMipmap(prefilterMap);
}

HDRSky::~HDRSky() {
    glDeleteFramebuffers(1, &captureFramebuffer);
    glDeleteRenderbuffers(1, &captureRenderbuffer);

    glDeleteFramebuffers(1, &skyboxFramebuffer);

    glDeleteTextures(1, &irradianceMap);
    glDeleteTextures(1, &environmentMap);
    glDeleteFramebuffers(1, &prefilterFramebuffer);

    unitCube.destroy();

}

void HDRSky::execute(const std::string& filepath) {
    stbi_set_flip_vertically_on_load(true);
    int w, h, ch;
    float* data = stbi_loadf(filepath.c_str(), &w, &h, &ch, 3);

    if (!data) {
        std::puts("failed to load envionrment map");
        return;
    }

    unsigned int hdrTexture;
    glCreateTextures(GL_TEXTURE_2D, 1, &hdrTexture);
    glTextureStorage2D(hdrTexture, 1, GL_RGB16F, w, h);
    glTextureSubImage2D(hdrTexture, 0, 0, 0, w, h, GL_RGB, GL_FLOAT, data);
    glTextureParameteri(hdrTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(hdrTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(hdrTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(hdrTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);

    glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 views[] = {
       glm::lookAtRH(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
       glm::lookAtRH(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
       glm::lookAtRH(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
       glm::lookAtRH(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
       glm::lookAtRH(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
       glm::lookAtRH(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    unitCube.vertexBuffer.bind();
    unitCube.indexBuffer.bind();

    equiToCubemapShader.bind();
    equiToCubemapShader["projection"] = projection;
    
    glBindTextureUnit(0, hdrTexture);

    glViewport(0, 0, 512, 512); 
    glBindFramebuffer(GL_FRAMEBUFFER, captureFramebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRenderbuffer);
    
    for (unsigned int i = 0; i < 6; ++i) {
        equiToCubemapShader["view"] = views[i];
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, environmentMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDrawElements(GL_TRIANGLES, unitCube.indices.size(), GL_UNSIGNED_INT, nullptr);
    }

    convoluteShader.bind();
    convoluteShader.getUniform("proj") = projection;
    
    glBindTextureUnit(0, environmentMap);
    
    glViewport(0, 0, 32, 32);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, convRenderbuffer);

    for (unsigned int i = 0; i < 6; i++) {
        convoluteShader.getUniform("view") = views[i];
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDrawElements(GL_TRIANGLES, unitCube.indices.size(), GL_UNSIGNED_INT, nullptr);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDeleteTextures(1, &hdrTexture);

    prefilterShader.bind();
    prefilterShader.getUniform("proj") = projection;

    glBindTextureUnit(0, environmentMap);

    glBindFramebuffer(GL_FRAMEBUFFER, prefilterFramebuffer);
    const unsigned int maxMipLevels = 5;
    for (unsigned int mip = 0; mip < maxMipLevels; mip++) {
        unsigned int mipWidth = 128 * std::pow(0.5, mip);
        unsigned int mipHeight = 128 * std::pow(0.5, mip);

        glViewport(0, 0, mipWidth, mipHeight);

        const float roughness = (float)mip / (float)(maxMipLevels - 1);
        prefilterShader.getUniform("roughness") = roughness;
        for (unsigned int i = 0; i < 6; i++) {
            prefilterShader.getUniform("view") = views[i];
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterMap, mip);
            glClear(GL_COLOR_BUFFER_BIT);

            glDrawElements(GL_TRIANGLES, unitCube.indices.size(), GL_UNSIGNED_INT, nullptr);
        }
    }
}

void HDRSky::renderEnvironmentMap(Viewport& viewport, unsigned int colorTarget, unsigned int depthTarget) {
    glBindFramebuffer(GL_FRAMEBUFFER, skyboxFramebuffer);
    glNamedFramebufferTexture(skyboxFramebuffer, GL_COLOR_ATTACHMENT0, colorTarget, 0);
    glNamedFramebufferDrawBuffer(skyboxFramebuffer, GL_COLOR_ATTACHMENT0);
    glNamedFramebufferTexture(skyboxFramebuffer, GL_DEPTH_ATTACHMENT, depthTarget, 0);

    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);

    skyboxShader.bind();
    skyboxShader.getUniform("proj") = viewport.getCamera().getProjection();
    skyboxShader.getUniform("view") = glm::mat4(glm::mat3(viewport.getCamera().getView()));

    glBindTextureUnit(0, prefilterMap);

    unitCube.vertexBuffer.bind();
    unitCube.indexBuffer.bind();
    glDrawElements(GL_TRIANGLES, unitCube.indices.size(), GL_UNSIGNED_INT, nullptr);

    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
}

} // renderpass
} // namespace Raekor