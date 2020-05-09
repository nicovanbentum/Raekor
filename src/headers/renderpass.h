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
    ShadowMap(uint32_t width, uint32_t height);
    void execute(Scene& scene, Camera& sunCamera);

private:
    glShader shader;
    glFramebuffer framebuffer;
    glUniformBuffer uniformBuffer;

public:
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

class DeferredLighting {
private:
    struct {
        glm::mat4 view, projection;
        glm::mat4 lightSpaceMatrix;
        glm::vec4 DirViewPos;
        glm::vec4 DirLightPos;
        glm::vec4 pointLightPos;
        unsigned int renderFlags = 0b00000001;
    } uniforms;

public:
    struct {
        float farPlane = 25.0f;
        float minBias = 0.000f, maxBias = 0.0f;
        glm::vec4 sunColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec3 bloomThreshold { 2.0f, 2.0f, 2.0f };
    } settings;


    DeferredLighting(Viewport& viewport);
    void execute(DeprecatedScene& scene, Viewport& viewport, ShadowMap* shadowMap, OmniShadowMap* omniShadowMap, 
                    GeometryBuffer* GBuffer, ScreenSpaceAmbientOcclusion* ambientOcclusion, Mesh* quad);
    void resize(Viewport& viewport);

private:
    glShader shader;
    glFramebuffer framebuffer;
    glRenderbuffer renderbuffer;
    glUniformBuffer uniformBuffer;

public:
    glTexture2D result;
    glTexture2D bloomHighlights;
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
    glRenderbuffer renderbuffer;
    glUniformBuffer uniformBuffer;

public:
    glTexture2D result;
};

//////////////////////////////////////////////////////////////////////////////////

class Voxelization {
public:
    Voxelization(uint32_t width, uint32_t height, uint32_t depth);
    void execute(Scene& scene, Viewport& viewport);

private:
    glShader shader;

public:
    uint32_t result;
};

//////////////////////////////////////////////////////////////////////////////////

class VoxelizationDebug {
public:
    VoxelizationDebug(Viewport& viewport);
    void execute(Viewport& viewport, uint32_t voxelMap, Mesh* cube, Mesh* quad);
    void resize(Viewport& viewport);

private:
    glShader basicShader;
    glShader voxelTracedShader;
    glRenderbuffer cubeTexture;
    glFramebuffer voxelVisFramebuffer;
    glFramebuffer cubeBackfaceFramebuffer;
    glFramebuffer cubeFrontfaceFramebuffer;

public:
    glTexture2D result;
    glTexture2D cubeBack;
    glTexture2D cubeFront;
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

} // renderpass
} // raekor
