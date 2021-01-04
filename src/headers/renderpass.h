#pragma once

#include "timer.h"
#include "shader.h"
#include "buffer.h"
#include "components.h"

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
         float depthBiasConstant = 1.25f;
         float depthBiasSlope = 1.75f;
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

    uint32_t readEntity(GLint x, GLint y);

    void createResources(Viewport& viewport);
    void deleteResources();

private:
    glShader shader;
    ShaderHotloader hotloader;
    unsigned int GBuffer;
  
public:
    unsigned int albedoTexture, normalTexture, materialTexture, entityTexture;
    unsigned int depthTexture;
};

class WorldIcons {
public:
    WorldIcons(Viewport& viewport) {
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
        glTextureParameteri(lightTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(lightTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(lightTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(lightTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(lightTexture, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        createResources(viewport);
    }

    void createResources(Viewport& viewport) {
        glCreateFramebuffers(1, &framebuffer);
    }

    void destroyResources() {
        glDeleteRenderbuffers(1, &framebuffer);
    }

    void execute(entt::registry& scene, Viewport& viewport, unsigned int screenTexture, unsigned int entityTexture) {
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

            if ((cosTheta < 0.9990) && (cosTheta > -0.9999)) {
                model = glm::rotate(model, acos(cosTheta), upAux);
            }

            model = glm::scale(model, glm::vec3(glm::length(V) * 0.05f));
            
            shader.getUniform("mvp") = vp * model;

            shader.getUniform("entity") = entt::to_integral(entity);
            shader.getUniform("world_position") = transform.position;
            
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glEnable(GL_CULL_FACE);
    }

private:
    glShader shader;
    unsigned int framebuffer;

    unsigned int lightTexture;
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
    void execute(Viewport& viewport, GeometryBuffer* geometryPass);

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
    void execute(Viewport& viewport, unsigned int highlights);
    void createResources(Viewport& viewport);
    void deleteResources();

private:
    void gaussianBlurLod(uint32_t texture, uint32_t level, uint32_t width, uint32_t height);

    glShader blurShader;
    glShader upsampleShader;
    std::vector<glm::ivec2> mipResolutions;
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
    void execute(unsigned int scene, unsigned int bloom);

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

class VoxelizationDebug {
public:
    ~VoxelizationDebug();
    
    // naive geometry shader impl
    VoxelizationDebug(Viewport& viewport); // naive geometry shader impl
    
    // fast cube rendering using a technique from https://twitter.com/SebAaltonen/status/1315982782439591938/photo/1
    VoxelizationDebug(Viewport& viewport, uint32_t voxelTextureSize);
    
    
    void execute(Viewport& viewport, unsigned int input, Voxelization* voxels);
    void execute2(Viewport& viewport, unsigned int input, Voxelization* voxels);

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
        glm::vec3 bloomThreshold{ 0.2126f , 0.7152f , 0.0722f };
    } settings;

    ~DeferredLighting();
    DeferredLighting(Viewport& viewport);

    void execute(entt::registry& sscene, Viewport& viewport, ShadowMap* shadowMap, OmniShadowMap* omniShadowMap,
        GeometryBuffer* GBuffer, ScreenSpaceAmbientOcclusion* ambientOcclusion, Voxelization* voxels);

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

class Skinning {
public:
    Skinning();
    void execute(ecs::MeshComponent& mesh, ecs::MeshAnimationComponent& anim);

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

    void execute(Viewport& viewport, bool shouldClear);

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

} // renderpass
} // raekor
