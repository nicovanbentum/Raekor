#pragma once

#include "timer.h"
#include "shader.h"
#include "components.h"
#include "camera.h"

namespace Raekor {

class Scene;

class GLTimer {
public:
    GLTimer() {
        glGenQueries(2, queries);
        glBeginQuery(GL_TIME_ELAPSED, queries[1]);
        glEndQuery(GL_TIME_ELAPSED);
    }

    ~GLTimer() {
        glDeleteQueries(2, queries);
    }

    void Begin() {
        glBeginQuery(GL_TIME_ELAPSED, queries[index]);
    }

    void End() {
        glEndQuery(GL_TIME_ELAPSED);
        glGetQueryObjectui64v(queries[!index], GL_QUERY_RESULT_NO_WAIT, &time);
        index = !index;
    }

    float GetMilliseconds() {
        return time * 0.000001f;
    }

private:
    GLuint64 time;
    GLuint index = 0;
    GLuint queries[2];
};

class ShadowMap {
    friend class GLRenderer;
    friend class ViewportWidget;

    struct Split {
        float split;
        float radius;
    };

 public:
     struct {
         float depthBiasConstant = 1.25f;
         float depthBiasSlope = 1.75f;
         float cascadeSplitLambda = 0.98f;
     } settings;

     struct {
         glm::mat4 lightMatrix;
         glm::mat4 modelMatrix;
     } uniforms;

    ~ShadowMap();
    ShadowMap(const Viewport& viewport, uint32_t width, uint32_t height);

    void updatePerspectiveConstants(const Viewport& viewport);

    void updateCascades(const Scene& scene, const Viewport& viewport);
    void render(const Viewport& viewport, const Scene& scene);
    void renderCascade(const Viewport& viewport, GLuint framebuffer);

private:
    glShader shader;
    glShader debugShader;
    GLuint framebuffer;
    GLuint uniformBuffer;

    int& lockRadius = ConVars::create("r_lockCascadeRadius", 0);

public:
    GLuint cascades;
    glm::vec4 m_splits;
    std::array<Split, 4> splits;
    std::array<glm::mat4, 4> matrices;
};

//////////////////////////////////////////////////////////////////////////////////

class GBuffer {
    friend class GLRenderer;
    friend class ViewportWidget;

    struct {
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 model;
        glm::vec4 colour;
        float metallic;
        float roughness;
        uint32_t entity;
    } uniforms;

public:
    uint32_t culled = 0;

    GBuffer(const Viewport& viewport);
    ~GBuffer();

    void render(const Scene& scene, const Viewport& viewport);

    uint32_t readEntity(GLint x, GLint y);

    void createRenderTargets(const Viewport& viewport);
    void destroyRenderTargets();

    GLuint getFramebuffer() { return framebuffer; }

    GLuint albedoTexture, normalTexture, materialTexture, entityTexture;
private:
    glShader shader;
    GLuint framebuffer;
    GLuint uniformBuffer;

    GLuint pbo;
    void* entity;
  
public:
    GLuint depthTexture;
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

//////////////////////////////////////////////////////////////////////////////////

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

    unsigned int blurTexture;
    unsigned int bloomTexture;
    unsigned int blurFramebuffer;
    unsigned int bloomFramebuffer;
    unsigned int highlightsFramebuffer;

private:
    glShader blurShader;
    GLuint uniformBuffer;
};

//////////////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////////////

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

    glm::mat4 px, py, pz;
    glShader shader;
    glShader mipmapShader;
    glShader opacityFixShader;

    GLuint uniformBuffer;

public:
    int size;
    float worldSize = 150.0f;
    GLuint result;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

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
    
    // fast cube rendering using a technique from https://twitter.com/SebAaltonen/status/1315982782439591938/photo/1
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
    glIndexBuffer indexBuffer;
    glVertexBuffer vertexBuffer;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class DebugLines {
    friend class GLRenderer;

    struct {
        glm::mat4 projection;
        glm::mat4 view;
    } uniforms;

public:
    friend class GLRenderer;

    DebugLines();
    ~DebugLines();
    
    void render(const Scene& scene, const Viewport& viewport, GLuint texture, GLuint renderBuffer);

private:
    glShader shader;
    GLuint frameBuffer;
    glVertexBuffer vertexBuffer;
    GLuint uniformBuffer;

    std::vector<glm::vec3> points;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class Skinning {
    friend class GLRenderer;

public:
    Skinning();
    void compute(const Mesh& mesh, const Skeleton& anim);

private:
    glShader computeShader;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////////////////////////////

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
                const Voxelize& voxels);

    float getTimeMs() { return timer.GetMilliseconds(); }

    void createRenderTargets(const Viewport& viewport);
    void destroyRenderTargets();

private:
    glShader shader;
    GLuint framebuffer;

    GLuint uniformBuffer;
    GLuint uniformBuffer2;

    GLuint brdfLUT;
    glShader brdfLUTshader;
    GLuint brdfLUTframebuffer;

    GLTimer timer;

public:
    GLuint result;
    GLuint bloomHighlights;
};

class Atmosphere {
    friend class GLRenderer;

    struct {
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

    void render(const Viewport& viewport, const Scene& scene, GLuint out, GLuint depth);


private:
    glShader shader;
    GLuint framebuffer;
    GLuint uniformBuffer;
};

} // raekor
