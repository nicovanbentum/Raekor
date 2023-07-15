#pragma once


#include "Editor/pch.h"
#include "Raekor/gui.h"
#include "Raekor/application.h"
#include "Raekor/timer.h"
#include "Editor/widget.h"


namespace Raekor {

struct Sphere {
    alignas(16) glm::vec3 origin;
    alignas(16) glm::vec3 colour;
    alignas(4) float roughness;
    alignas(4) float metalness;
    alignas(4) float radius;
};


class RayTracingOneWeekend final : public RenderPass {
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

    void CreateRenderTargets(const Viewport& viewport);
    void DestroyRenderTargets();

    bool shaderChanged() { return true; }

    std::vector<Sphere> spheres;

private:
    Timer rayTimer;
    GLShader shader;

    GLuint sphereBuffer;
    GLuint uniformBuffer;

public:
    GLuint result, finalResult;
};


class RayTraceApp : public Application {
public:
    RayTraceApp();
    virtual ~RayTraceApp() = default;

    virtual void OnUpdate(float dt) override;
    virtual void OnEvent(const SDL_Event& ev) override {}

    bool drawSphereProperties(Sphere& sphere);

private:
    bool sceneChanged = false;

    unsigned int activeSphere = 0;
    unsigned int activeScreenTexture = 0;

    //GLRenderer renderer;
    std::vector<std::shared_ptr<IWidget>> widgets;
    std::unique_ptr<RayTracingOneWeekend> rayTracePass;
};


} // raekor
