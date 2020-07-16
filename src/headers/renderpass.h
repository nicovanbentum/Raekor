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
         glm::vec<2, float> planes = { 1.0f, 200.0f };
         float size = 100.0f;
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
    ECS::Entity pick(uint32_t x, uint32_t y);

private:
    glShader shader;
    glFramebuffer GBuffer;

    ShaderHotloader hotloader;
  
public:
    glRenderbuffer GDepthBuffer;
    glTexture2D albedoTexture, normalTexture, positionTexture, materialTexture;
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
        float gamma = 2.2f;
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
    void execute(Scene& scene, Viewport& viewport, ShadowMap* shadowmap);

private:
    void computeMipmaps(glTexture3D& texture);

    void correctOpacity(glTexture3D& texture);

    int size;
    glm::mat4 px, py, pz;
    glShader shader;
    glShader mipmapShader;
    glShader opacityFixShader;
    ShaderHotloader hotloader;

public:
    float worldSize = 150.0f;
    glTexture3D result;
};

//////////////////////////////////////////////////////////////////////////////////

class VoxelizationDebug {
public:
    VoxelizationDebug(Viewport& viewport);
    void execute(Viewport& viewport, glTexture2D& input, Voxelization* voxels);
    void resize(Viewport& viewport);

private:
    glFramebuffer frameBuffer;
    glRenderbuffer renderBuffer;
    glShader shader;
};

class BoundingBoxDebug {
public:
    BoundingBoxDebug(Viewport& viewport);
    void execute(Scene& scene, Viewport& viewport, glTexture2D& texture, glRenderbuffer& renderBuffer, ECS::Entity active);
    void resize(Viewport& viewport);

private:
    glShader shader;
    glFramebuffer frameBuffer;
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
    ForwardLightingPass(Viewport& viewport);
    void execute(Viewport& viewport, Scene& scene, Voxelization* voxels, ShadowMap* shadowmap);
    void resize(Viewport& viewport);

    ECS::Entity pick(uint32_t x, uint32_t y) {
        int id;
        framebuffer.bind();
        glReadPixels(x, y, 1, 1, GL_STENCIL_INDEX, GL_INT, &id);
        framebuffer.unbind();
        return id;
    }

private:
    glShader shader;
    glFramebuffer framebuffer;
    glRenderbuffer renderbuffer;
    glUniformBuffer uniformBuffer;

    ShaderHotloader hotloader;

public:
    int culled = 0;
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

class SkyPass {
public:
    struct {
        float time = 0.0f;
        float cirrus = 0.4f;
        float cumulus = 0.8f;
    } settings;

    SkyPass(Viewport& viewport);

    void execute(Viewport& viewport, Mesh* quad);

private:
    glShader shader;
    ShaderHotloader hotloader;
    glFramebuffer framebuffer;

public:
    glTexture2D result;
};

class Skinning {
public:
    Skinning() {
        std::vector<Shader::Stage> stages;
        stages.emplace_back(Shader::Type::COMPUTE, "shaders\\OpenGL\\skinning.comp");
        computeShader.reload(stages.data(), stages.size());
        hotloader.watch(&computeShader, stages.data(), stages.size());
    }

    void execute(ECS::MeshComponent& mesh, ECS::MeshAnimationComponent& anim) {

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

private:
    glShader computeShader;
    ShaderHotloader hotloader;
};

} // renderpass
} // raekor
