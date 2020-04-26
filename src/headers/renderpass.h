#pragma once

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
    GLResourceBuffer uniformBuffer;

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
    GeometryBuffer(uint32_t width, uint32_t height);
    void execute(Scene& scene);
    void resize(uint32_t width, uint32_t height);

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
    ScreenSpaceAmbientOcclusion(uint32_t renderWidth, uint32_t renderHeight);
    void execute(Scene& scene, GeometryBuffer* geometryPass, Mesh* quad);
    void resize(uint32_t renderWidth, uint32_t renderHeight);

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


    DeferredLighting(uint32_t width, uint32_t height) {
        // load shaders from disk
        Shader::Stage vertex(Shader::Type::VERTEX, "shaders\\OpenGL\\main.vert");
        Shader::Stage frag(Shader::Type::FRAG, "shaders\\OpenGL\\main.frag");
        std::array<Shader::Stage, 2> modelStages = { vertex, frag };
        shader.reload(modelStages.data(), modelStages.size());

        // init render targets
        result.bind();
        result.init(width, height, Format::RGBA_F16);
        result.setFilter(Sampling::Filter::Bilinear);
        result.unbind();

        bloomHighlights.bind();
        bloomHighlights.init(width, height, Format::RGBA_F16);
        bloomHighlights.setFilter(Sampling::Filter::Bilinear);
        bloomHighlights.unbind();

        renderbuffer.init(width, height, GL_DEPTH32F_STENCIL8);

        framebuffer.bind();
        framebuffer.attach(result, GL_COLOR_ATTACHMENT0);
        framebuffer.attach(bloomHighlights, GL_COLOR_ATTACHMENT1);
        framebuffer.attach(renderbuffer, GL_DEPTH_STENCIL_ATTACHMENT);
        framebuffer.unbind();

        // init uniform buffer
        uniformBuffer.setSize(sizeof(uniforms));
    }

    void execute(Scene& scene, ShadowMap* shadowMap, OmniShadowMap* omniShadowMap, GeometryBuffer* GBuffer, ScreenSpaceAmbientOcclusion* ambientOcclusion,  Mesh* quad) {
        // bind the main framebuffer
        framebuffer.bind();
        Renderer::Clear({ 0.0f, 0.0f, 0.0f, 1.0f });

        // set uniforms
        shader.bind();
        shader.getUniform("sunColor") = settings.sunColor;
        shader.getUniform("minBias") = settings.minBias;
        shader.getUniform("maxBias") = settings.maxBias;
        shader.getUniform("farPlane") = settings.farPlane;
        shader.getUniform("bloomThreshold") = settings.bloomThreshold;

        // bind textures to shader binding slots
        shadowMap->result.bindToSlot(0);
        omniShadowMap->result.bindToSlot(1);
        GBuffer->positionTexture.bindToSlot(2);
        GBuffer->albedoTexture.bindToSlot(3);
        GBuffer->normalTexture.bindToSlot(4);
        ambientOcclusion->result.bindToSlot(5);

        // update the uniform buffer CPU side
        uniforms.view = scene.camera.getView();
        uniforms.projection = scene.camera.getProjection();
        uniforms.pointLightPos = glm::vec4(scene.pointLight.position, 1.0f);
        uniforms.DirLightPos = glm::vec4(scene.sunCamera.getPosition(), 1.0);
        uniforms.DirViewPos = glm::vec4(scene.camera.getPosition(), 1.0);
        uniforms.lightSpaceMatrix = scene.sunCamera.getProjection() * scene.sunCamera.getView();

        // update uniform buffer GPU side
        uniformBuffer.update(&uniforms, sizeof(uniforms));
        uniformBuffer.bind(0);

        // perform lighting pass and unbind
        quad->render();
        framebuffer.unbind();
    }

private:
    glShader shader;
    glFramebuffer framebuffer;
    glRenderbuffer renderbuffer;
    GLResourceBuffer uniformBuffer;

public:
    glTexture2D result;
    glTexture2D bloomHighlights;
};

} // renderpass
} // raekor
