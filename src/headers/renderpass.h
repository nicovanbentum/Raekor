#pragma once

#include "ecs.h"
#include "scene.h"
#include "mesh.h"
#include "timer.h"

namespace Raekor {
namespace RenderPass {

class ShadowMap {
 public:
    struct {
        glm::mat4 cameraMatrix;
    } uniforms;

     struct {
         glm::vec<2, float> planes = { 1.0f, 200.0f };
         float size = 50.0f;
     } settings;

    ~ShadowMap();
    ShadowMap(uint32_t width, uint32_t height);

    void execute(entt::registry& scene);

private:
    glShader shader;
    unsigned int framebuffer;
    glUniformBuffer uniformBuffer;

public:
    unsigned int result;
};

//////////////////////////////////////////////////////////////////////////////////

class OmniShadowMap {
public:
    struct {
        uint32_t width, height;
        float nearPlane = 0.1f, farPlane = 25.0f;
    } settings;

    OmniShadowMap(uint32_t width, uint32_t height);
    void execute(entt::registry& scene, const glm::vec3& lightPosition);

private:
    glShader shader;
    unsigned int depthCubeFramebuffer;

public:
    unsigned int result;
};

//////////////////////////////////////////////////////////////////////////////////

class GeometryBuffer {
public:
    uint32_t culled = 0;

    ~GeometryBuffer();
    GeometryBuffer(Viewport& viewport);

    void execute(entt::registry& scene, Viewport& viewport);

    entt::entity pick(uint32_t x, uint32_t y);

    void createResources(Viewport& viewport);
    void deleteResources();

private:
    glShader shader;
    ShaderHotloader hotloader;
    unsigned int GBuffer;
  
public:
    unsigned int GDepthBuffer;
    unsigned int albedoTexture, normalTexture, positionTexture, materialTexture;
};

//////////////////////////////////////////////////////////////////////////////////

class ScreenSpaceAmbientOcclusion {
public:
    struct {
        float samples = 64.0f;
        float bias = 0.025f, power = 2.5f;
    } settings;

    ~ScreenSpaceAmbientOcclusion();
    ScreenSpaceAmbientOcclusion(Viewport& viewport);
    void execute(Viewport& viewport, GeometryBuffer* geometryPass, Mesh* quad);

    void createResources(Viewport& viewport);
    void deleteResources();

private:
    unsigned int noiseTexture;
    glShader shader;
    glShader blurShader;
    unsigned int framebuffer;
    unsigned int blurFramebuffer;

public:
    unsigned int result;
    unsigned int preblurResult;

private:
    glm::vec2 noiseScale;
    std::vector<glm::vec3> ssaoKernel;
};

//////////////////////////////////////////////////////////////////////////////////

class Bloom {
public:
    ~Bloom();
    Bloom(Viewport& viewport);
    void execute(unsigned int scene, unsigned int highlights, Mesh* quad);
    void createResources(Viewport& viewport);
    void deleteResources();

private:
    glShader blurShader;
    glShader bloomShader;
    unsigned int blurTextures[2];
    unsigned int blurBuffers[2];
    unsigned int resultFramebuffer;
    
public:
    unsigned int result;
};

//////////////////////////////////////////////////////////////////////////////////

class Tonemapping {
public:
    struct {
        float exposure = 1.0f;
        float gamma = 2.2f;
    } settings;

    ~Tonemapping();
    Tonemapping(Viewport& viewport);
    void execute(unsigned int scene, Mesh* quad);

    void createResources(Viewport& viewport);
    void deleteResources();

private:
    glShader shader;
    unsigned int framebuffer;
    glUniformBuffer uniformBuffer;

public:
    unsigned int result;
};

//////////////////////////////////////////////////////////////////////////////////

class Voxelization {
public:
    Voxelization(int size);
    void execute(entt::registry& scene, Viewport& viewport, ShadowMap* shadowmap);

private:
    void computeMipmaps(unsigned int texture);

    void correctOpacity(unsigned int texture);

    int size;
    glm::mat4 px, py, pz;
    glShader shader;
    glShader mipmapShader;
    glShader opacityFixShader;
    ShaderHotloader hotloader;

public:
    float worldSize = 150.0f;
    unsigned int result;
};

//////////////////////////////////////////////////////////////////////////////////

class VoxelizationDebug {
public:
    ~VoxelizationDebug();
    VoxelizationDebug(Viewport& viewport);

    void execute(Viewport& viewport, unsigned int input, Voxelization* voxels);

    void createResources(Viewport& viewport);
    void deleteResources();

private:
    unsigned int frameBuffer;
    unsigned int renderBuffer;
    glShader shader;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class BoundingBoxDebug {
public:
    ~BoundingBoxDebug();
    BoundingBoxDebug(Viewport& viewport);

    void execute(entt::registry& scene, Viewport& viewport, unsigned int texture, unsigned int renderBuffer, entt::entity active);

    void createResources(Viewport& viewport);
    void deleteResources();

private:
    glShader shader;
    unsigned int frameBuffer;
    glVertexBuffer vertexBuffer;
    glIndexBuffer indexBuffer;

public:
    unsigned int result;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class ForwardLighting {
private:
    struct {
        glm::mat4 view, projection;
        glm::mat4 lightSpaceMatrix;
        glm::vec4 cameraPosition;
        ecs::DirectionalLightComponent::ShaderBuffer dirLights[1];
        ecs::PointLightComponent::ShaderBuffer pointLights[10];
        unsigned int renderFlags = 0b00000001;
    } uniforms;

public:
    ~ForwardLighting();
    ForwardLighting(Viewport& viewport);

    void execute(Viewport& viewport, entt::registry& scene, Voxelization* voxels, ShadowMap* shadowmap);

    void createResources(Viewport& viewport);
    void deleteResources();

    entt::entity pick(uint32_t x, uint32_t y);

private:
    glShader shader;
    unsigned int framebuffer;
    unsigned int renderbuffer;
    glUniformBuffer uniformBuffer;

    ShaderHotloader hotloader;

public:
    int culled = 0;
    unsigned int result;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class DeferredLighting {
private:
    struct {
        glm::mat4 view, projection;
        glm::mat4 lightSpaceMatrix;
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
        glm::vec3 bloomThreshold{ 2.0f, 2.0f, 2.0f };
    } settings;

    ~DeferredLighting();
    DeferredLighting(Viewport& viewport);

    void execute(entt::registry& sscene, Viewport& viewport, ShadowMap* shadowMap, OmniShadowMap* omniShadowMap,
        GeometryBuffer* GBuffer, ScreenSpaceAmbientOcclusion* ambientOcclusion, Voxelization* voxels, Mesh* quad);

    void createResources(Viewport& viewport);
    void deleteResources();

private:
    glShader shader;
    unsigned int framebuffer;
    glUniformBuffer uniformBuffer;

    ShaderHotloader hotloader;

public:
    unsigned int result;
    unsigned int bloomHighlights;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class Sky {
public:
    struct {
        float time = 0.0f;
        float cirrus = 0.4f;
        float cumulus = 0.8f;
    } settings;

    Sky(Viewport& viewport);

    void execute(Viewport& viewport, Mesh* quad);

private:
    glShader shader;
    ShaderHotloader hotloader;
    unsigned int framebuffer;

public:
    unsigned int result;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class Skinning {
public:
    Skinning();
    void execute(ecs::MeshComponent& mesh, ecs::MeshAnimationComponent& anim);

private:
    glShader computeShader;
    ShaderHotloader hotloader;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class Environment {
public:
    Environment() = default;
    void execute(const std::string& file, Mesh* unitCube);

private:
    unsigned int envCubemap;
    unsigned int irradianceCubemap;

    unsigned int captureFramebuffer;
    unsigned int captureRenderbuffer;
    glShader toCubemapShader;
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

    void execute(Viewport& viewport, bool shouldClear);

    void createResources(Viewport& viewport);
    void deleteResources();

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

} // renderpass
} // raekor
