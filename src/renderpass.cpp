#include "pch.h"
#include "renderpass.h"
#include "ecs.h"
#include "camera.h"
#include "timer.h"

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

void GeometryBuffer::execute(entt::registry& scene, Viewport& viewport, unsigned int depthTextureEXT) {
    hotloader.changed();

    glBindFramebuffer(GL_FRAMEBUFFER, GBuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (depthTextureEXT) {
        int w, h;
        int miplevel = 0;
        glGetTextureLevelParameteriv(depthTextureEXT, 0, GL_TEXTURE_WIDTH, &w);
        glGetTextureLevelParameteriv(depthTextureEXT, 0, GL_TEXTURE_HEIGHT, &h);

        glNamedFramebufferTexture(GBuffer, GL_DEPTH_ATTACHMENT, depthTextureEXT, 0);
    }

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
    std::array<float, 4> pixel;
    glGetTextureSubImage(materialTexture, 0, x, y,
        0, 1, 1, 1, GL_RGBA, GL_FLOAT, sizeof(pixel), pixel.data());
    return static_cast<uint32_t>(pixel[2]);
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
    glTextureStorage2D(materialTexture, 1, GL_RGBA32F, viewport.size.x, viewport.size.y);
    glTextureParameteri(materialTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(materialTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateTextures(GL_TEXTURE_2D, 1, &depthTexture);
    glTextureStorage2D(depthTexture, 1, GL_DEPTH_COMPONENT32F, viewport.size.x, viewport.size.y);
    glTextureParameteri(depthTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(depthTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateFramebuffers(1, &GBuffer);
    glNamedFramebufferTexture(GBuffer, GL_COLOR_ATTACHMENT0, normalTexture, 0);
    glNamedFramebufferTexture(GBuffer, GL_COLOR_ATTACHMENT1, albedoTexture, 0);
    glNamedFramebufferTexture(GBuffer, GL_COLOR_ATTACHMENT2, materialTexture, 0);

    std::array<GLenum, 4> colorAttachments = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
    glNamedFramebufferDrawBuffers(GBuffer, static_cast<GLsizei>(colorAttachments.size()), colorAttachments.data());
    glNamedFramebufferTexture(GBuffer, GL_DEPTH_ATTACHMENT, depthTexture, 0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void GeometryBuffer::deleteResources() {
    std::array<unsigned int, 4> textures = { albedoTexture, normalTexture, materialTexture, depthTexture };
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

void DeferredLighting::execute(entt::registry& sscene, Viewport& viewport, GeometryBuffer* GBuffer, Voxelization* voxels, unsigned int depthTextureEXT, unsigned int shadowTexture) {
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

    // update uniforms GPU side
    uniformBuffer.update(&uniforms, sizeof(uniforms));

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

    // TODO: why on earth does this need to be here?? If I just do:
    // inverse(projection * view) in the shader we get shadow flickering.
    // Probably something to do with how the uniform buffer is updated?
    shader.getUniform("invViewProjection") = glm::inverse(uniforms.projection * uniforms.view);

    // bind textures to shader binding slots
    glBindTextureUnit(0, shadowTexture);

    glBindTextureUnit(3, GBuffer->albedoTexture);
    glBindTextureUnit(4, GBuffer->normalTexture);

    glBindTextureUnit(6, voxels->result);
    glBindTextureUnit(7, GBuffer->materialTexture);

    if (depthTextureEXT) {
        glBindTextureUnit(8, depthTextureEXT);
    } else {
        glBindTextureUnit(8, GBuffer->depthTexture);
    }




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
    glTextureStorage2D(bloomHighlights, 5, GL_RGBA16F, std::max(16u, viewport.size.x), std::max(16u, viewport.size.y));
    glTextureParameteri(bloomHighlights, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(bloomHighlights, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(bloomHighlights, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(bloomHighlights, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(bloomHighlights, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glGenerateTextureMipmap(bloomHighlights);

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
    auto blurStage = Shader::Stage(Shader::Type::COMPUTE, "shaders\\OpenGL\\gaussian.comp");
    blurShader.reload(&blurStage, 1);

    auto upsampleStage = Shader::Stage(Shader::Type::COMPUTE, "shaders\\OpenGL\\upsample.comp");
    upsampleShader.reload(&upsampleStage, 1);

    createResources(viewport);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Bloom::execute(Viewport& viewport, unsigned int highlights) {
    if (viewport.size.x < 16.0f || viewport.size.y < 16.0f) {
        return;
    }
    
    for (int i = 0; i < 10; i++) {
        auto resolution = mipResolutions[0];
        gaussianBlurLod(highlights, 0, resolution.x, resolution.y);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Bloom::createResources(Viewport& viewport) {
    for (unsigned int level = 0; level < 5; level++) {
        auto resolution = viewport.size / static_cast<unsigned int>(std::pow(2, level));
        mipResolutions.push_back(resolution);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void Bloom::deleteResources() {
    mipResolutions.clear();
}

void Bloom::gaussianBlurLod(uint32_t texture, uint32_t level, uint32_t width, uint32_t height) {
    blurShader.bind();
    glBindImageTexture(0, texture, level, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);
    glBindTextureUnit(1, texture);
    
    blurShader.getUniform("direction") = glm::vec2(1, 0);
    glDispatchCompute(width / 16, height / 16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    blurShader.getUniform("direction") = glm::vec2(1, 0);
    glDispatchCompute(width / 16, height / 16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
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
    tonemapStages.emplace_back(Shader::Type::FRAG, "shaders\\OpenGL\\HDR.frag");
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
            if (material->albedo) glBindTextureUnit(0, material->albedo);
        } else {
            glBindTextureUnit(0, ecs::MaterialComponent::Default.albedo);
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

//////////////////////////////////////////////////////////////////////////////////////////////////

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

RayTracedShadows::RayTracedShadows(Viewport& viewport) {
    scat.init(viewport.size.x, viewport.size.y);
    createResources(viewport);

    readyHandle = scat.getReadySemaphoreHandle();
    doneHandle = scat.getDoneSemaphoreHandle();

    glGenSemaphoresEXT(1, &readySemaphore);
    glGenSemaphoresEXT(1, &completeSemaphore);
    glImportSemaphoreWin32HandleEXT(readySemaphore, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, readyHandle);
    glImportSemaphoreWin32HandleEXT(completeSemaphore, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, doneHandle);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

RayTracedShadows::~RayTracedShadows() {
    CloseHandle(readyHandle);
    CloseHandle(doneHandle);
    CloseHandle(depthHandle);
    CloseHandle(shadowHandle);

    //scat.destroyTextures();
    glDeleteTextures(1, &depthTexture);
    glDeleteMemoryObjectsEXT(1, &depthTextureMemory);

    glDeleteTextures(1, &shadowTexture);
    glDeleteMemoryObjectsEXT(1, &shadowTextureMemory);

    glDeleteSemaphoresEXT(1, &readySemaphore);
    glDeleteSemaphoresEXT(1, &completeSemaphore);

    scat.destroy();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void RayTracedShadows::createAccelerationStructure(entt::registry& scene) {
    // set the vertex input state
    scat.setVertexOffset(0);
    scat.setVertexStride(sizeof(float) * 3);
    scat.setIndexFormat(scatter::IndexFormat::UINT32);
    scat.setVertexFormat(scatter::VertexFormat::R32G32B32_SFLOAT);

    // loop over all scene geometry and make a single BLAS per mesh
    // scatter has a single TLAS of BLAS's, with single instances
    auto view = scene.view<ecs::MeshComponent, ecs::TransformComponent>();

    for (entt::entity entity : view) {
        ecs::MeshComponent& mesh = view.get<ecs::MeshComponent>(entity);
        ecs::TransformComponent& transform = view.get<ecs::TransformComponent>(entity);

        auto& asc = scene.emplace<ecs::AccelStructureComponent>(entity);

        asc.BLAS = scat.addMesh(mesh.positions.data(), mesh.indices.data(),
            (unsigned int)mesh.positions.size(), (unsigned int)mesh.indices.size());

        // GLM is column major, vulkan wants row major
        auto transposed = glm::transpose(transform.matrix); 

        scat.addInstance(asc.BLAS, glm::value_ptr(transposed));
    }

    // build the acceleration structure
    scat.build();
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void RayTracedShadows::createResources(Viewport& viewport) {
    scat.createTextures(viewport.size.x, viewport.size.y);

    depthHandle = scat.getDepthTextureMemoryhandle();
    const auto depthSize = scat.getDepthTextureMemorySize();

    shadowHandle = scat.getShadowTextureMemoryHandle();
    auto shadowSize = scat.getShadowTextureMemorySize();

    glCreateMemoryObjectsEXT(1, &depthTextureMemory);
    glImportMemoryWin32HandleEXT(depthTextureMemory, depthSize, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, depthHandle);

    glCreateTextures(GL_TEXTURE_2D, 1, &depthTexture);
    glTextureStorageMem2DEXT(depthTexture, 1, GL_DEPTH_COMPONENT32F, viewport.size.x, viewport.size.y, depthTextureMemory, 0);
    glTextureParameteri(depthTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(depthTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateMemoryObjectsEXT(1, &shadowTextureMemory);
    glImportMemoryWin32HandleEXT(shadowTextureMemory, shadowSize, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, shadowHandle);

    glCreateTextures(GL_TEXTURE_2D, 1, &shadowTexture);
    glTextureStorageMem2DEXT(shadowTexture, 1, GL_RGBA8, viewport.size.x, viewport.size.y, shadowTextureMemory, 0);
    glTextureParameteri(shadowTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(shadowTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void RayTracedShadows::destroyResources() {
    scat.destroyTextures();

    glDeleteTextures(1, &depthTexture);
    glDeleteMemoryObjectsEXT(1, &depthTextureMemory);

    glDeleteTextures(1, &shadowTexture);
    glDeleteMemoryObjectsEXT(1, &shadowTextureMemory);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void RayTracedShadows::updateTLAS(entt::registry& scene) {
    scat.clearInstances();

    auto view = scene.view<ecs::TransformComponent, ecs::AccelStructureComponent>();

    for (entt::entity entity : view) {
        ecs::TransformComponent& transform = view.get<ecs::TransformComponent>(entity);
        ecs::AccelStructureComponent& accelStructure = view.get<ecs::AccelStructureComponent>(entity);

        // GLM is column major, vulkan wants row major
        auto transposed = glm::transpose(transform.matrix);

        scat.addInstance(accelStructure.BLAS, glm::value_ptr(transposed));
    }

    scat.build();
}

void RayTracedShadows::execute(Viewport& viewport, ecs::DirectionalLightComponent& light) {
    // give scatter the camera's inverse view projection matrix
    auto invViewProj = glm::inverse(viewport.getCamera().getProjection() * viewport.getCamera().getView());
    scat.setInverseViewProjectionMatrix(glm::value_ptr(invViewProj));

    // set the light direction
    scat.setLightDirection(light.buffer.direction.x, light.buffer.direction.y, light.buffer.direction.z);

    GLuint textures[2] = { shadowTexture, depthTexture };
    GLenum vkLayout[2] = { GL_LAYOUT_GENERAL_EXT, GL_LAYOUT_GENERAL_EXT };
    glSignalSemaphoreEXT(readySemaphore, 0, nullptr, 2, textures, vkLayout);

    // call vulkan submit
    scat.submit(viewport.size.x, viewport.size.y);

    GLenum glLayout[2] = { GL_LAYOUT_SHADER_READ_ONLY_EXT, GL_LAYOUT_DEPTH_STENCIL_ATTACHMENT_EXT };
    glWaitSemaphoreEXT(completeSemaphore, 0, nullptr, 2, &depthTexture, glLayout);
}

} // renderpass
} // namespace Raekor