#pragma once

#include "Raekor/timer.h"
#include "Raekor/components.h"
#include "shader.h"
#include "buffer.h"

namespace Raekor {

class Scene;
class Viewport;

class GLTimer {
public:
    GLTimer();
    ~GLTimer();

    void begin();
    void end();

    float getMilliseconds() const;

private:
    GLuint64 time = 0;
    GLuint index = 0;
    std::array<GLuint, 4> queries;
};



class ShadowMap {
    struct Cascade {
        float split;
        float radius;
        glm::mat4 matrix;
    };

 public:
     struct {
         uint32_t resolution = 4096;
         uint32_t nrOfCascades = 4;
         float depthBiasConstant = 1.25f;
         float depthBiasSlope = 3.0f; // 1.75f; 
         float cascadeSplitLambda = 0.89f; // 0.98f;
     } settings;

     struct {
         glm::mat4 lightMatrix;
         glm::mat4 modelMatrix;
     } uniforms;

    ~ShadowMap();
    ShadowMap(const Viewport& viewport);

    void updatePerspectiveConstants(const Viewport& viewport);
    void updateCascades(const Scene& scene, const Viewport& viewport);
    void render(const Viewport& viewport, const Scene& scene);
    void renderCascade(const Viewport& viewport, GLuint framebuffer);

private:
    GLuint framebuffer;
    GLuint uniformBuffer;

    glShader shader;
    glShader debugShader;
    
    glVertexLayout vertexLayout;

public:
    GLuint texture;
    std::vector<Cascade> cascades;
};



class GBuffer {
    friend class GLRenderer;
    friend class ViewportWidget;

    struct {
        glm::mat4 prevViewProj;
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 model;
        glm::vec4 colour;
        glm::vec2 jitter;
        glm::vec2 prevJitter;
        float metallic;
        float roughness;
        uint32_t entity;
    } uniforms;

public:
    uint32_t culled = 0;

    GBuffer(const Viewport& viewport);
    ~GBuffer();

    uint32_t readEntity(GLint x, GLint y);

    void render(const Scene& scene, const Viewport& viewport, uint32_t frameNr);
    void createRenderTargets(const Viewport& viewport);
    void destroyRenderTargets();

public:
    GLuint framebuffer;
    GLuint depthTexture;
    GLuint albedoTexture;
    GLuint normalTexture;
    GLuint materialTexture;
    GLuint entityTexture;
    GLuint velocityTexture;

private:
    glShader shader;
    GLuint uniformBuffer;

    glVertexLayout vertexLayout;

    GLuint pbo;
    void* entity;
  
};



class Icons {
    friend class GLRenderer;

    struct {
        glm::mat4 mvp;
        glm::vec4 world_position;
        uint32_t entity;
    } uniforms;

public:
    Icons(const Viewport& viewport);
    ~Icons();

    void createRenderTargets(const Viewport& viewport);
    void destroyRenderTargets();

    void render(const Scene& scene, const Viewport& viewport, GLuint screenTexture, GLuint entityTexture);

private:
    glShader shader;
    GLuint framebuffer;
    GLuint lightTexture;
    GLuint uniformBuffer;
};



class Bloom {
    friend class GLRenderer;

    struct {
        glm::vec2 direction;
    } uniforms;

public:
    ~Bloom();
    Bloom(const Viewport& viewport);
    void render(const Viewport& viewport, GLuint highlights);
    void createRenderTargets(const Viewport& viewport);
    void destroyRenderTargets();

    GLuint blurTexture;
    GLuint bloomTexture;
    GLuint blurFramebuffer;
    GLuint bloomFramebuffer;
    GLuint highlightsFramebuffer;

private:
    glShader blurShader;
    GLuint uniformBuffer;
};

class Tonemap {
    friend class GLRenderer;
    friend class ViewportWidget;

public:
    struct {
        float exposure = 1.0f;
        float gamma = 2.2f;
    } settings;

    ~Tonemap();
    Tonemap(const Viewport& viewport);
    void render(GLuint scene, GLuint bloom);

    void createRenderTargets(const Viewport& viewport);
    void destroyRenderTargets();

private:
    glShader shader;
    GLuint framebuffer;
    GLuint uniformBuffer;

public:
    GLuint result;
};



class Voxelize {
    struct Uniforms {
        glm::mat4 px, py, pz;
        std::array<glm::mat4, 4> shadowMatrices;
        glm::mat4 view;
        glm::mat4 model;
        glm::vec4 shadowSplits;
        glm::vec4 colour;
    } uniforms;

public:
    Voxelize(uint32_t size);
    void render(const Scene& scene, const Viewport& viewport, const ShadowMap& shadowmap);

private:
    void computeMipmaps(GLuint texture);
    void correctOpacity(GLuint texture);

    glShader shader;
    glShader mipmapShader;
    glShader opacityFixShader;
    glVertexLayout vertexLayout;

    GLuint uniformBuffer;
    glm::mat4 px, py, pz;

public:
    int size;
    float worldSize = 80.0f;
    GLuint result;
};



class VoxelizeDebug {
    struct {
        glm::mat4 p;
        glm::mat4 mv;
        glm::vec4 cameraPosition;
    } uniforms;

public:
    ~VoxelizeDebug();
    
    // naive geometry shader impl
    VoxelizeDebug(const Viewport& viewport); // naive geometry shader impl
    
    // fast cube rendering from https://twitter.com/SebAaltonen/status/1315982782439591938/photo/1
    VoxelizeDebug(const Viewport& viewport, uint32_t voxelTextureSize);
    
    void render(const Viewport& viewport, GLuint input, const Voxelize& voxels);
    void execute2(const Viewport& viewport, GLuint input, const Voxelize& voxels);

    void createRenderTargets(const Viewport& viewport);
    void destroyRenderTargets();

private:
    glShader shader;
    GLuint frameBuffer, renderBuffer;
    GLuint uniformBuffer;

    uint32_t indexCount;
    uint32_t indexBuffer;
};



class DebugLines {
    friend class GLRenderer;

    struct {
        glm::mat4 projection;
        glm::mat4 view;
    } uniforms;

public:
    DebugLines();
    ~DebugLines();

    void render(const Viewport& viewport, GLuint colorAttachment, GLuint depthAttachment);

private:
    glShader shader;
    GLuint frameBuffer;
    GLuint uniformBuffer;

    uint32_t vertexBuffer;
    glVertexLayout vertexLayout;

    std::vector<glm::vec3> points;
};



class Skinning {
    friend class GLRenderer;

public:
    Skinning();
    void compute(const Mesh& mesh, const Skeleton& anim);

private:
    glShader computeShader;
};



struct Sphere {
    alignas(16) glm::vec3 origin;
    alignas(16) glm::vec3 colour;
    alignas(4) float roughness;
    alignas(4) float metalness;
    alignas(4) float radius;
};

class RayTracingOneWeekend {
    struct {
        glm::vec4 position;
        glm::mat4 projection;
        glm::mat4 view;
        float iTime;
        uint32_t sphereCount;
        uint32_t doUpdate;
    } uniforms;

public:
    RayTracingOneWeekend(const Viewport& viewport);
    ~RayTracingOneWeekend();

    void compute(const Viewport& viewport, bool shouldClear);

    void createRenderTargets(const Viewport& viewport);
    void destroyRenderTargets();

    bool shaderChanged() { return true; }

    std::vector<Sphere> spheres;

private:
    Timer rayTimer;
    glShader shader;

    GLuint sphereBuffer;
    GLuint uniformBuffer;

public:
    GLuint result, finalResult;
};



class Atmosphere {
    friend class GLRenderer;

    struct {
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 invViewProj;
        glm::vec4 cameraPos;
        glm::vec4 sunlightDir;
        glm::vec4 sunlightColor;
    } uniforms;

public:
    Atmosphere(const Viewport& viewport);
    ~Atmosphere();

    void createRenderTargets(const Viewport& viewport);
    void destroyRenderTargets();

    void computeCubemaps(const Viewport& viewport, const Scene& scene);

    GLuint environmentCubemap;
    GLuint convolvedCubemap;

private:
    GLuint framebuffer;
    GLuint uniformBuffer;

    glShader computeShader;
    glShader convoluteShader;
};



class DeferredShading {
    friend class GLRenderer;

private:
    struct Uniforms {
        glm::mat4 view, projection;
        std::array<glm::mat4, 4> shadowMatrices;
        glm::vec4 shadowSplits;
        glm::vec4 cameraPosition;
        DirectionalLight dirLight;
        PointLight pointLights[10];
    } uniforms;

    struct Uniforms2 {
        glm::mat4 invViewProjection;
        glm::vec4 bloomThreshold;
        float voxelWorldSize;
        int pointLightCount;
        int directionalLightCount;
    } uniforms2;

public:
    struct {
        float farPlane = 25.0f;
        float minBias = 0.000f, maxBias = 0.0f;
        glm::vec4 sunColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec3 bloomThreshold{ 0.2126f , 0.7152f , 0.0722f };
    } settings;

    ~DeferredShading();
    DeferredShading(const Viewport& viewport);

    void render(const Scene& sscene, 
                const Viewport& viewport, 
                const ShadowMap& shadowMap,
                const GBuffer& GBuffer, 
                const Atmosphere& atmosphere,
                const Voxelize& voxels);

    void createRenderTargets(const Viewport& viewport);
    void destroyRenderTargets();

public:
    GLuint result;
    GLuint bloomHighlights;

private:
    glShader shader;
    GLuint framebuffer;

    GLuint uniformBuffer;
    GLuint uniformBuffer2;

    GLuint brdfLUT;
    glShader brdfLUTshader;
    GLuint brdfLUTframebuffer;
};



class TAAResolve {
public:
    TAAResolve(const Viewport& viewport);

    GLuint render(const Viewport& viewport, const GBuffer& gbuffer, const DeferredShading& shading, uint32_t frameNr);

    void createRenderTargets(const Viewport& viewport);
    void destroyRenderTargets();

public:
    GLuint resultBuffer, historyBuffer;

private:
    glShader shader;
};

} // raekor
