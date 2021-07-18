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
 public:
     struct {
         float depthBiasConstant = 1.25f;
         float depthBiasSlope = 1.75f;
         float cascadeSplitLambda = 0.98f;
     } settings;

    ~ShadowMap();
    ShadowMap(uint32_t width, uint32_t height);

    void render(const Viewport& viewport, const Scene& scene);

private:
    glShader shader;
    unsigned int framebuffer;

public:
    unsigned int cascades;
    glm::vec4 m_splits;
    std::array<glm::mat4, 4> matrices;
};

//////////////////////////////////////////////////////////////////////////////////

class GBuffer {
public:
    uint32_t culled = 0;

    ~GBuffer();
    GBuffer(const Viewport& viewport);

    void render(const Scene& scene, const Viewport& viewport);

    uint32_t readEntity(GLint x, GLint y);

    void createRenderTargets(const Viewport& viewport);
    void destroyRenderTargets();

    GLuint getFramebuffer() { return framebuffer; }

    GLuint albedoTexture, normalTexture, materialTexture, entityTexture;
private:
    glShader shader;
    ShaderHotloader hotloader;
    GLuint framebuffer;
  
public:
    GLuint depthTexture;
};

class Icons {
public:
    Icons(const Viewport& viewport);

    void createRenderTargets(const Viewport& viewport);
    void destroyResources();

    void render(const Scene& scene, const Viewport& viewport, GLuint screenTexture, GLuint entityTexture);

private:
    glShader shader;
    GLuint framebuffer;
    GLuint lightTexture;
};

//////////////////////////////////////////////////////////////////////////////////

class Bloom {
public:
    ~Bloom();
    Bloom(const Viewport& viewport);
    void render(const Viewport& viewport, unsigned int highlights);
    void createRenderTargets(const Viewport& viewport);
    void destroyRenderTargets();

    unsigned int blurTexture;
    unsigned int bloomTexture;
    unsigned int blurFramebuffer;
    unsigned int bloomFramebuffer;
    unsigned int highlightsFramebuffer;

private:
    glShader blurShader;
};

//////////////////////////////////////////////////////////////////////////////////

class Tonemap {
public:
    struct {
        float exposure = 1.0f;
        float gamma = 2.2f;
    } settings;

    ~Tonemap();
    Tonemap(const Viewport& viewport);
    void render(unsigned int scene, unsigned int bloom);

    void createRenderTargets(const Viewport& viewport);
    void destroyRenderTargets();

private:
    glShader shader;
    unsigned int framebuffer;
    glUniformBuffer uniformBuffer;
    ShaderHotloader hotloader;

public:
    unsigned int result;
};

//////////////////////////////////////////////////////////////////////////////////

class Voxelize {
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
    ShaderHotloader hotloader;

public:
    int size;
    float worldSize = 150.0f;
    GLuint result;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class VoxelizeDebug {
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

    uint32_t indexCount;
    glIndexBuffer indexBuffer;
    glVertexBuffer vertexBuffer;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class DebugLines {
public:
    friend class GLRenderer;

    DebugLines();
    ~DebugLines();
    
    void render(const Scene& scene, const Viewport& viewport, GLuint texture, GLuint renderBuffer);

private:
    glShader shader;
    GLuint frameBuffer;
    glVertexBuffer vertexBuffer;

    std::vector<Vertex> points;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class Skinning {
public:
    Skinning();
    void compute(const ecs::MeshComponent& mesh, const ecs::AnimationComponent& anim);

private:
    glShader computeShader;
    ShaderHotloader hotloader;
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
public:
    RayTracingOneWeekend(const Viewport& viewport);
    ~RayTracingOneWeekend();

    void compute(const Viewport& viewport, bool shouldClear);

    void createRenderTargets(const Viewport& viewport);
    void destroyRenderTargets();

    bool shaderChanged() { return hotloader.changed(); }

    std::vector<Sphere> spheres;

private:
    Timer rayTimer;
    glShader shader;
    ShaderHotloader hotloader;

    GLuint sphereBuffer;

public:
    GLuint result, finalResult;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class DeferredShading {
private:
    struct Uniforms {
        glm::mat4 view, projection;
        std::array<glm::mat4, 4> shadowMatrices;
        glm::vec4 shadowSplits;
        glm::vec4 cameraPosition;
        ecs::DirectionalLightComponent dirLight;
        ecs::PointLightComponent pointLights[10];
        unsigned int renderFlags = 0b00000001;
    } uniforms;

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
    glUniformBuffer uniformBuffer;

    GLTimer timer;
    ShaderHotloader hotloader;

public:
    GLuint result;
    GLuint bloomHighlights;
};

class Atmosphere {
public:
    Atmosphere(const Viewport& viewport);
    ~Atmosphere();

    void createRenderTargets(const Viewport& viewport);
    void destroyResources();

    void render(const Viewport& viewport, const Scene& scene, GLuint out, GLuint depth);


private:
    glShader shader;
    ShaderHotloader hotloader;
    GLuint framebuffer;
};

} // raekor
