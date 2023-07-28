#pragma once

#include "shader.h"
#include "Raekor/timer.h"
#include "Raekor/components.h"

namespace Raekor {
    class Scene;
    class Viewport;
}

namespace Raekor::GL {

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



class RenderPass {
public:
    class ScopedTimer {
    public:
        ScopedTimer(const char* inName, RenderPass* inThis, bool inEnabled) : m_This(inThis), m_Enabled(inEnabled) { 
            if (m_Enabled) m_This->m_Timer.begin(); 
        }
        ~ScopedTimer() { if (m_Enabled) m_This->m_Timer.end(); }

    private:
        bool m_Enabled = false;
        RenderPass* m_This = nullptr;
    };

    virtual void CreateRenderTargets(const Viewport& inViewport) = 0;
    virtual void DestroyRenderTargets() = 0;

private:
    const char* m_Name;
    GLTimer m_Timer;
};



class ShadowMap final : public RenderPass {
    struct Cascade {
        float split;
        float radius;
        glm::mat4 matrix;
    };

 public:
     static constexpr auto MAX_NR_OF_CASCADES = 4u;

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

    virtual void CreateRenderTargets(const Viewport& viewport) override {}
    virtual void DestroyRenderTargets() override {}

    void updatePerspectiveConstants(const Viewport& viewport);
    void updateCascades(const Scene& scene, const Viewport& viewport);
    void Render(const Viewport& viewport, const Scene& scene);
    void renderCascade(const Viewport& viewport, GLuint framebuffer);

private:
    GLuint framebuffer;
    GLuint uniformBuffer;

    GLShader shader;
    GLShader debugShader;

public:
    GLuint texture;
    std::vector<Cascade> cascades;
};



class GBuffer final : public RenderPass {
    friend class Renderer;

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
        float mLODFade = 0.0f;
    } uniforms;

public:
    uint32_t culled = 0;

    GBuffer(const Viewport& viewport);
    ~GBuffer();

    uint32_t readEntity(GLint x, GLint y);

    void Render(const Scene& scene, const Viewport& viewport, uint32_t m_FrameNr);
    virtual void CreateRenderTargets(const Viewport& viewport) override;
    virtual void DestroyRenderTargets() override;

public:
    GLuint framebuffer;
    GLuint depthTexture;
    GLuint albedoTexture;
    GLuint normalTexture;
    GLuint materialTexture;
    GLuint entityTexture;
    GLuint velocityTexture;

private:
    GLShader shader;
    GLuint uniformBuffer;

    GLuint pbo;
    void* entity;
  
};



class Icons final : public RenderPass {
    friend class Renderer;

    struct {
        glm::mat4 mvp;
        glm::vec4 world_position;
        uint32_t entity;
    } uniforms;

public:
    Icons(const Viewport& viewport);
    ~Icons();

    void CreateRenderTargets(const Viewport& viewport);
    void DestroyRenderTargets();

    void Render(const Scene& scene, const Viewport& viewport, GLuint screenTexture, GLuint entityTexture);

private:
    GLShader shader;
    GLuint framebuffer;
    GLuint lightTexture;
    GLuint uniformBuffer;
};



class Bloom : public RenderPass {
    friend class Renderer;

    struct {
        glm::vec2 direction;
    } uniforms;

public:
    ~Bloom();
    Bloom(const Viewport& viewport);
    void Render(const Viewport& viewport, GLuint highlights);
    void CreateRenderTargets(const Viewport& viewport);
    void DestroyRenderTargets();

    GLuint blurTexture;
    GLuint bloomTexture;
    GLuint blurFramebuffer;
    GLuint bloomFramebuffer;
    GLuint highlightsFramebuffer;

private:
    GLShader blurShader;
    GLuint uniformBuffer;
};

class Tonemap final : public RenderPass {
    friend class Renderer;

public:
    struct {
        float exposure = 1.0f;
        float gamma = 2.2f;
    } settings;

    ~Tonemap();
    Tonemap(const Viewport& viewport);
    void Render(GLuint scene, GLuint m_Bloom);

    void CreateRenderTargets(const Viewport& viewport);
    void DestroyRenderTargets();

private:
    GLShader shader;
    GLuint framebuffer;
    GLuint uniformBuffer;

public:
    GLuint result;
};



class Voxelize final : public RenderPass {
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
    void Render(const Scene& scene, const Viewport& viewport, const ShadowMap& shadowmap);

    virtual void CreateRenderTargets(const Viewport& inViewport) override {}
    virtual void DestroyRenderTargets() override {}

private:
    void computeMipmaps(GLuint texture);
    void correctOpacity(GLuint texture);

    GLShader shader;
    GLShader mipmapShader;
    GLShader opacityFixShader;

    GLuint uniformBuffer;
    glm::mat4 px, py, pz;

public:
    int size;
    float worldSize = 80.0f;
    GLuint result;
};



class VoxelizeDebug final : public RenderPass {
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
    
    void Render(const Viewport& viewport, GLuint input, const Voxelize& voxels);
    void execute2(const Viewport& viewport, GLuint input, const Voxelize& voxels);

    void CreateRenderTargets(const Viewport& viewport);
    void DestroyRenderTargets();

private:
    GLShader shader;
    GLuint frameBuffer, renderBuffer;
    GLuint uniformBuffer;

    uint32_t indexCount;
    uint32_t indexBuffer;
};



class DebugLines final : public RenderPass {
    friend class Renderer;

    struct {
        glm::mat4 projection;
        glm::mat4 view;
    } uniforms;

public:
    DebugLines();
    ~DebugLines();

    virtual void CreateRenderTargets(const Viewport& inViewport) override {}
    virtual void DestroyRenderTargets() override {}

    void Render(const Viewport& viewport, GLuint colorAttachment, GLuint depthAttachment);

private:
    GLShader shader;
    GLuint frameBuffer;
    GLuint uniformBuffer;

    uint32_t vertexBuffer;
    std::vector<glm::vec3> points;
};



class Skinning final : public RenderPass {
    friend class Renderer;

public:
    Skinning();

    virtual void CreateRenderTargets(const Viewport& inViewport) override {}
    virtual void DestroyRenderTargets() override {}

    void compute(const Mesh& mesh, const Skeleton& anim);

private:
    GLShader computeShader;
};


class Atmosphere final : public RenderPass {
    friend class Renderer;

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

    void CreateRenderTargets(const Viewport& viewport);
    void DestroyRenderTargets();

    void computeCubemaps(const Viewport& viewport, const Scene& scene);

    GLuint environmentCubemap;
    GLuint convolvedCubemap;

private:
    GLuint framebuffer;
    GLuint uniformBuffer;

    GLShader computeShader;
    GLShader convoluteShader;
};



class DeferredShading final : public RenderPass {
    friend class Renderer;

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

    void Render(const Scene& sscene, 
                const Viewport& viewport, 
                const ShadowMap& shadowMap,
                const GBuffer& GBuffer, 
                const Atmosphere& m_Atmosphere,
                const Voxelize& voxels);

    void CreateRenderTargets(const Viewport& viewport);
    void DestroyRenderTargets();

public:
    GLuint result;
    GLuint bloomHighlights;

private:
    GLShader shader;
    GLuint framebuffer;

    GLuint uniformBuffer;
    GLuint uniformBuffer2;

    GLuint brdfLUT;
    GLShader brdfLUTshader;
    GLuint brdfLUTframebuffer;
};



class TAAResolve final : public RenderPass {
public:
    TAAResolve(const Viewport& viewport);

    GLuint Render(const Viewport& viewport, const GBuffer& m_GBuffer, const DeferredShading& shading, uint32_t m_FrameNr);

    void CreateRenderTargets(const Viewport& viewport);
    void DestroyRenderTargets();

public:
    GLuint resultBuffer, historyBuffer;

private:
    GLShader shader;
};


class ImGuiPass final : public RenderPass {
    void CreateRenderTargets(const Viewport& viewport) {}
    void DestroyRenderTargets() {}
}; // Just so we can time it


} // raekor
