#pragma once

#include "ecs.h"
#include "scene.h"
#include "mesh.h"
#include "texture.h"
#include "framebuffer.h"

namespace Raekor {
namespace RenderPass {

class ShadowMap {
private:
    struct {
        glm::mat4 cameraMatrix;
    } uniforms;

 public:
     struct {
         glm::vec2 planes = { 1.0, 20.0f };
         float size = 16.0f;
     } settings;

    ShadowMap(uint32_t width, uint32_t height);
    void execute(Scene& scene);

private:
    glShader shader;
    glFramebuffer framebuffer;
    glUniformBuffer uniformBuffer;

public:
    Camera sunCamera;
    glTexture2D result;
};

//////////////////////////////////////////////////////////////////////////////////

class OmniShadowMap {
public:
    struct {
        uint32_t width, height;
        float nearPlane = 0.1f, farPlane = 25.0f;
    } settings;

    OmniShadowMap(uint32_t width, uint32_t height);
    void execute(Scene& scene, const glm::vec3& lightPosition);

private:
    glShader shader;
    glFramebuffer depthCubeFramebuffer;

public:
    glTextureCube result;
};

//////////////////////////////////////////////////////////////////////////////////

class GeometryBuffer {
public:
    uint32_t culled = 0;

    GeometryBuffer(Viewport& viewport);
    void execute(Scene& scene, Viewport& viewport);
    void resize(Viewport& viewport);

    ECS::Entity pick(uint32_t x, uint32_t y) {
        int id;
        GBuffer.bind();
        glReadPixels(x, y, 1, 1, GL_STENCIL_INDEX, GL_INT, &id);
        GBuffer.unbind();
        return id;
    }

private:
    glShader shader;
    glFramebuffer GBuffer;
    glRenderbuffer GDepthBuffer;
  
public:
    glTexture2D albedoTexture, normalTexture, positionTexture;
};

//////////////////////////////////////////////////////////////////////////////////

class ScreenSpaceAmbientOcclusion {
public:
    struct {
        float samples = 64.0f;
        float bias = 0.025f, power = 2.5f;
    } settings;

    ScreenSpaceAmbientOcclusion(Viewport& viewport);
    void execute(Viewport& viewport, GeometryBuffer* geometryPass, Mesh* quad);
    void resize(Viewport& viewport);

private:
    glTexture2D noise;
    glShader shader;
    glShader blurShader;
    glFramebuffer framebuffer;
    glFramebuffer blurFramebuffer;

public:
    glTexture2D result;
    glTexture2D preblurResult;

private:
    glm::vec2 noiseScale;
    std::vector<glm::vec3> ssaoKernel;
};

//////////////////////////////////////////////////////////////////////////////////

class Bloom {
public:
    Bloom(Viewport& viewport);
    void execute(glTexture2D& scene, glTexture2D& highlights, Mesh* quad);
    void resize(Viewport& viewport);

private:
    glShader blurShader;
    glShader bloomShader;
    glTexture2D blurTextures[2];
    glFramebuffer blurBuffers[2];
    glFramebuffer resultFramebuffer;
    
public:
    glTexture2D result;
};

//////////////////////////////////////////////////////////////////////////////////

class Tonemapping {
public:
    struct {
        float exposure = 1.0f;
        float gamma = 1.8f;
    } settings;

    Tonemapping(Viewport& viewport);
    void resize(Viewport& viewport);
    void execute(glTexture2D& scene, Mesh* quad);

private:
    glShader shader;
    glFramebuffer framebuffer;
    glUniformBuffer uniformBuffer;

public:
    glTexture2D result;
};

//////////////////////////////////////////////////////////////////////////////////

class Voxelization {
public:
    Voxelization(int size);
    void execute(Scene& scene, Viewport& viewport);

private:
    int size;
    glm::mat4 px, py, pz;
    glShader shader;

public:
    uint32_t result;
};

//////////////////////////////////////////////////////////////////////////////////

class VoxelizationDebug {
public:
    VoxelizationDebug(Viewport& viewport);
    void execute(Viewport& viewport, glTexture2D& input, uint32_t voxelMap);
    void resize(Viewport& viewport);

private:
    glFramebuffer frameBuffer;
    glRenderbuffer renderBuffer;
    glShader shader;
};

class BoundingBoxDebug {
public:
    BoundingBoxDebug(Viewport& viewport);
    void execute(Scene& scene, Viewport& viewport, glTexture2D& texture, ECS::Entity active);
    void resize(Viewport& viewport);

private:
    glShader shader;
    glFramebuffer frameBuffer;
    glRenderbuffer renderBuffer;
    glVertexBuffer vertexBuffer;
    glIndexBuffer indexBuffer;

public:
    glTexture2D result;
};

class ForwardLightingPass {
private:
    struct {
        glm::mat4 view, projection;
        glm::mat4 lightSpaceMatrix;
        glm::vec4 cameraPosition;
        ECS::DirectionalLightComponent::ShaderBuffer dirLights[1];
        ECS::PointLightComponent::ShaderBuffer pointLights[10];
        unsigned int renderFlags = 0b00000001;
    } uniforms;

public:
    ForwardLightingPass(Viewport& viewport) {
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

        framebuffer.bind();
        framebuffer.attach(result, GL_COLOR_ATTACHMENT0);
        framebuffer.attach(renderbuffer, GL_DEPTH_STENCIL_ATTACHMENT);
        framebuffer.unbind();

        // init uniform buffer
        uniformBuffer.setSize(sizeof(uniforms));
    }

    void execute(Viewport& viewport, Scene& scene, Voxelization* voxels, ShadowMap* shadowmap) {
        hotloader.checkForUpdates();

        // update the uniform buffer CPU side
        uniforms.view = viewport.getCamera().getView();
        uniforms.projection = viewport.getCamera().getProjection();

        for (uint32_t i = 0; i < scene.directionalLights.getCount() && i < ARRAYSIZE(uniforms.dirLights); i++) {
            auto entity = scene.directionalLights.getEntity(i);
            auto transform = scene.transforms.getComponent(entity);

            auto& light = scene.directionalLights[i];

            light.buffer.position = glm::vec4(transform->position, 1.0f);

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

        glBindTextureUnit(0, voxels->result);
        shadowmap->result.bindToSlot(3);
    
        Math::Frustrum frustrum;
        frustrum.update(viewport.getCamera().getProjection() * viewport.getCamera().getView(), true);

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

            mesh.vertexBuffer.bind();
            mesh.indexBuffer.bind();
            Renderer::DrawIndexed(mesh.indexBuffer.count);
        }

        framebuffer.unbind();
    }

    void resize(Viewport& viewport) {
        result.bind();
        result.init(viewport.size.x, viewport.size.y, Format::RGBA_F16);

        renderbuffer.init(viewport.size.x, viewport.size.y, GL_DEPTH32F_STENCIL8);
    }

private:
    glShader shader;
    glFramebuffer framebuffer;
    glRenderbuffer renderbuffer;
    glUniformBuffer uniformBuffer;

    ShaderHotloader hotloader;

public:
    glTexture2D result;
};

//////////////////////////////////////////////////////////////////////////////////

class DeferredLighting {
private:
    struct {
        glm::mat4 view, projection;
        glm::mat4 lightSpaceMatrix;
        glm::vec4 cameraPosition;
        ECS::DirectionalLightComponent::ShaderBuffer dirLights[1];
        ECS::PointLightComponent::ShaderBuffer pointLights[10];
        unsigned int renderFlags = 0b00000001;
    } uniforms;

public:
    struct {
        float farPlane = 25.0f;
        float minBias = 0.000f, maxBias = 0.0f;
        glm::vec4 sunColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec3 bloomThreshold{ 2.0f, 2.0f, 2.0f };
    } settings;


    DeferredLighting(Viewport& viewport);
    void execute(Scene& sscene, Viewport& viewport, ShadowMap* shadowMap, OmniShadowMap* omniShadowMap,
        GeometryBuffer* GBuffer, ScreenSpaceAmbientOcclusion* ambientOcclusion, Voxelization* voxels, Mesh* quad);
    void resize(Viewport& viewport);

private:
    glShader shader;
    glFramebuffer framebuffer;
    glUniformBuffer uniformBuffer;

    ShaderHotloader hotloader;

public:
    glTexture2D result;
    glTexture2D bloomHighlights;
};

} // renderpass
} // raekor
