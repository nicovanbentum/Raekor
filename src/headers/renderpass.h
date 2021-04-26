#pragma once

#include "timer.h"
#include "shader.h"
#include "components.h"
#include "camera.h"

namespace Raekor {

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
         float cascadeSplitLambda = 0.985f;
     } settings;

    ~ShadowMap();
    ShadowMap(uint32_t width, uint32_t height);

    void render(Viewport& viewport, entt::registry& scene);

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
    GBuffer(Viewport& viewport);

    void render(entt::registry& scene, Viewport& viewport);

    uint32_t readEntity(GLint x, GLint y);

    void createResources(Viewport& viewport);
    void deleteResources();

    unsigned int getFramebuffer() { return framebuffer; }

    unsigned int albedoTexture, normalTexture, materialTexture, entityTexture;
private:
    glShader shader;
    ShaderHotloader hotloader;
    unsigned int framebuffer;
  
public:
    unsigned int depthTexture;
};

class Icons {
public:
    Icons(Viewport& viewport);

    void createResources(Viewport& viewport);
    void destroyResources();

    void render(entt::registry& scene, Viewport& viewport, unsigned int screenTexture, unsigned int entityTexture);

private:
    glShader shader;
    unsigned int framebuffer;

    unsigned int lightTexture;
};

//////////////////////////////////////////////////////////////////////////////////

class Bloom {
public:
    ~Bloom();
    Bloom(Viewport& viewport);
    void render(Viewport& viewport, unsigned int highlights);
    void createResources(Viewport& viewport);
    void deleteResources();

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
    Tonemap(Viewport& viewport);
    void render(unsigned int scene, unsigned int bloom);

    void createResources(Viewport& viewport);
    void deleteResources();

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
    Voxelize(int size);
    void render(entt::registry& scene, Viewport& viewport, ShadowMap* shadowmap);

private:
    void computeMipmaps(unsigned int texture);

    void correctOpacity(unsigned int texture);

    glm::mat4 px, py, pz;
    glShader shader;
    glShader mipmapShader;
    glShader opacityFixShader;
    ShaderHotloader hotloader;

public:
    int size;
    float worldSize = 150.0f;
    unsigned int result;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class VoxelizeDebug {
public:
    ~VoxelizeDebug();
    
    // naive geometry shader impl
    VoxelizeDebug(Viewport& viewport); // naive geometry shader impl
    
    // fast cube rendering using a technique from https://twitter.com/SebAaltonen/status/1315982782439591938/photo/1
    VoxelizeDebug(Viewport& viewport, uint32_t voxelTextureSize);
    
    
    void render(Viewport& viewport, unsigned int input, Voxelize* voxels);
    void execute2(Viewport& viewport, unsigned int input, Voxelize* voxels);

    void createResources(Viewport& viewport);
    void deleteResources();

private:
    glShader shader;
    unsigned int frameBuffer;
    unsigned int renderBuffer;

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
    
    void render(entt::registry& scene, Viewport& viewport, unsigned int texture, unsigned int renderBuffer);

private:
    glShader shader;
    unsigned int frameBuffer;
    glVertexBuffer vertexBuffer;

    std::vector<Vertex> points;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class SkinCompute {
public:
    SkinCompute();
    void render(ecs::MeshComponent& mesh, ecs::MeshAnimationComponent& anim);

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

class RayCompute {
public:
    RayCompute(Viewport& viewport);
    ~RayCompute();

    void render(Viewport& viewport, bool shouldClear);

    void createResources(Viewport& viewport);
    void deleteResources();

    bool shaderChanged() { return hotloader.changed(); }

    std::vector<Sphere> spheres;

private:
    Timer rayTimer;
    glShader shader;
    ShaderHotloader hotloader;
    unsigned int sphereBuffer;

public:
    unsigned int result;
    unsigned int finalResult;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class DeferredShading {
private:
    struct {
        glm::mat4 view, projection;
        std::array<glm::mat4, 4> shadowMatrices;
        glm::vec4 shadowSplits;
        glm::vec4 cameraPosition;
        ecs::DirectionalLightComponent::ShaderBuffer dirLights[1];
        ecs::PointLightComponent::ShaderBuffer pointLights[10];
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
    DeferredShading(Viewport& viewport);

    void render(entt::registry& sscene, Viewport& viewport, ShadowMap* shadowMap,
        GBuffer* GBuffer, Voxelize* voxels);

    float getTimeMs() { return timer.GetMilliseconds(); }

    void createResources(Viewport& viewport);
    void deleteResources();

private:
    glShader shader;
    unsigned int framebuffer;
    glUniformBuffer uniformBuffer;

    GLTimer timer;
    ShaderHotloader hotloader;


public:
    unsigned int result;
    unsigned int bloomHighlights;
};

class Atmosphere {
public:
    Atmosphere(Viewport& viewport);
    ~Atmosphere();

    void createResources(Viewport& viewport);
    void destroyResources();

    void render(Viewport& viewport, entt::registry& scene, unsigned int out, unsigned int depth);


private:
    glShader shader;
    ShaderHotloader hotloader;
    unsigned int framebuffer;
};

} // raekor
