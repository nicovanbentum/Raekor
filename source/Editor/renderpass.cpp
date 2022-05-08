#include "pch.h"
#include "renderpass.h"
#include "Raekor/ecs.h"
#include "Raekor/camera.h"
#include "Raekor/timer.h"
#include "Raekor/scene.h"

namespace Raekor {

void BindInterleavedVertexLayout(const Mesh& mesh) {
    const bool has_uvs       = !mesh.uvs.empty();
    const bool has_normals   = !mesh.normals.empty();
    const bool has_tangents  = !mesh.tangents.empty();

    size_t uvs_offset = 0, normals_offset = 0, tangents_offset = 0;

    uint32_t stride = sizeof(decltype(mesh.positions)::value_type);

    if (has_uvs) { 
        uvs_offset = stride; 
        stride += sizeof(decltype(mesh.uvs)::value_type); 
    }

    if (has_normals) { 
        normals_offset = stride; 
        stride += sizeof(decltype(mesh.normals)::value_type); 
    }

    if (has_tangents) { 
        tangents_offset = stride; 
        stride += sizeof(decltype(mesh.tangents)::value_type); 
    }

    uint32_t attrib_id = 0;

    glEnableVertexAttribArray(attrib_id);
    glVertexAttribPointer(attrib_id++, decltype(mesh.positions)::value_type().length(),
        GL_FLOAT, GL_FALSE, (GLsizei)stride, (const void*)((intptr_t)0)
    );

    if (has_uvs) {
        glEnableVertexAttribArray(attrib_id);
        glVertexAttribPointer(attrib_id++, decltype(mesh.uvs)::value_type().length(),
            GL_FLOAT, GL_FALSE, (GLsizei)stride, (const void*)((intptr_t)uvs_offset)
        );
    }

    if (has_normals) {
        glEnableVertexAttribArray(attrib_id);
        glVertexAttribPointer(attrib_id++, decltype(mesh.normals)::value_type().length(),
            GL_FLOAT, GL_FALSE, (GLsizei)stride, (const void*)((intptr_t)normals_offset)
        );
    }

    if (has_tangents) {
        glEnableVertexAttribArray(attrib_id);
        glVertexAttribPointer(attrib_id++, decltype(mesh.tangents)::value_type().length(),
            GL_FLOAT, GL_FALSE, (GLsizei)stride, (const void*)((intptr_t)tangents_offset)
        );
    }
}


GLTimer::GLTimer() {
    glGenQueries(GLsizei(queries.size()), queries.data());
    for (auto query : queries) {
        glBeginQuery(GL_TIME_ELAPSED, query);
        glEndQuery(GL_TIME_ELAPSED);
    }
}


GLTimer::~GLTimer() {
    glDeleteQueries(GLsizei(queries.size()), queries.data());
}


void GLTimer::begin() {
    glBeginQuery(GL_TIME_ELAPSED, queries[index]);
}


void GLTimer::end() {
    glEndQuery(GL_TIME_ELAPSED);
    
    // check all the queries for results, except the one that just ended
    for (uint32_t i = 0; i < queries.size(); i++) {
        if (i == index) continue;
        glGetQueryObjectui64v(queries[i], GL_QUERY_RESULT_NO_WAIT, &time);
    }

    index = (index + 1) % queries.size();
}


float GLTimer::getMilliseconds() const {
    return time * 0.000001f;
}


ShadowMap::ShadowMap(const Viewport& viewport) {
    cascades.resize(settings.nrOfCascades);

    shader.compile({
        {Shader::Type::VERTEX, "assets\\system\\shaders\\OpenGL\\depth.vert"},
        {Shader::Type::FRAG, "assets\\system\\shaders\\OpenGL\\depth.frag"}
    });

    debugShader.compile({
        {Shader::Type::VERTEX, "assets\\system\\shaders\\OpenGL\\quad.vert"},
        {Shader::Type::FRAG, "assets\\system\\shaders\\OpenGL\\quad.frag"}
    });

    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &texture);
    glTextureStorage3D(texture, 1, GL_DEPTH_COMPONENT32F, settings.resolution, settings.resolution, settings.nrOfCascades);
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);

    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTextureParameterfv(texture, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTextureParameteri(texture, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTextureParameteri(texture, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);

    glCreateFramebuffers(1, &framebuffer);
    glNamedFramebufferTextureLayer(framebuffer, GL_DEPTH_ATTACHMENT, texture, 0, 0);
    glNamedFramebufferDrawBuffer(framebuffer, GL_NONE);
    glNamedFramebufferReadBuffer(framebuffer, GL_NONE);

    glCreateBuffers(1, &uniformBuffer);
    glNamedBufferStorage(uniformBuffer, sizeof(uniforms), NULL, GL_DYNAMIC_STORAGE_BIT);
}


ShadowMap::~ShadowMap() {
    glDeleteTextures(1, &texture);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteBuffers(1, &uniformBuffer);
}


void ShadowMap::updatePerspectiveConstants(const Viewport& viewport) {
    const float nearClip = viewport.GetCamera().GetNear();
    const float farClip = viewport.GetCamera().GetFar();

    const float clipRange = farClip - nearClip;

    const float minZ = nearClip;
    const float maxZ = nearClip + clipRange;

    const float range = maxZ - minZ;
    const float ratio = maxZ / minZ;

    std::vector<float> dists(settings.nrOfCascades);

    // Calculate split depths based on view camera frustum
    // Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
    for (uint32_t i = 0; i < settings.nrOfCascades; i++) {
        const float p = (i + 1) / float(settings.nrOfCascades);
        const float log = minZ * std::pow(ratio, p);
        const float uniform = minZ + range * p;
        const float d = settings.cascadeSplitLambda * (log - uniform) + uniform;
        dists[i] = (d - nearClip) / clipRange;
    }

    float lastSplitDist = 0.0;
    for (uint32_t i = 0; i < settings.nrOfCascades; i++) {
        float splitDist = dists[i];

        std::array frustumCorners = {
            glm::vec3(-1.0f,  1.0f, -1.0f),
            glm::vec3( 1.0f,  1.0f, -1.0f),
            glm::vec3( 1.0f, -1.0f, -1.0f),
            glm::vec3(-1.0f, -1.0f, -1.0f),
            glm::vec3(-1.0f,  1.0f,  1.0f),
            glm::vec3( 1.0f,  1.0f,  1.0f),
            glm::vec3( 1.0f, -1.0f,  1.0f),
            glm::vec3(-1.0f, -1.0f,  1.0f),
        };

        const glm::mat4 inverse = glm::inverse(viewport.GetCamera().GetProjection() * viewport.GetCamera().GetView());

        for (auto& corner : frustumCorners) {
            glm::vec4 invCorner = inverse * glm::vec4(corner, 1.0f);
            corner = invCorner / invCorner.w;
        }

        for (uint32_t i = 0; i < 4; i++) {
            const glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
            frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
            frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
        }

        glm::vec3 frustumCenter = glm::vec3(0.0f);
        for (const auto& corner : frustumCorners) {
            frustumCenter += corner;
        }
        frustumCenter /= 8.0f;

        cascades[i].radius = 0.0f;
        for (uint32_t j = 0; j < 8; j++) {
            float distance = glm::length(frustumCorners[j] - frustumCenter);
            cascades[i].radius = glm::max(cascades[i].radius, distance);
        }

        cascades[i].radius = std::ceil(cascades[i].radius * 16.0f) / 16.0f;
    }
}


void ShadowMap::updateCascades(const Scene& scene, const Viewport& viewport) {
    const auto lightView = scene.view<const DirectionalLight, const Transform>();
    auto lookDirection = glm::vec3(0.25f, -0.9f, 0.0f);

    if (lightView.begin() != lightView.end()) {
        const auto& lightTransform = lightView.get<const Transform>(lightView.front());
        lookDirection = static_cast<glm::quat>(lightTransform.rotation) * lookDirection;
    } else {
        // we rotate default light a little or else we get nan values in our view matrix
        lookDirection = static_cast<glm::quat>(glm::vec3(glm::radians(15.0f), 0, 0)) * lookDirection;
    }

    lookDirection = glm::clamp(lookDirection, { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f });

    const float nearClip = viewport.GetCamera().GetNear();
    const float farClip = viewport.GetCamera().GetFar();
    const float clipRange = farClip - nearClip;

    const float minZ = nearClip;
    const float maxZ = nearClip + clipRange;

    const float range = maxZ - minZ;
    const float ratio = maxZ / minZ;

    std::vector<float> dists(settings.nrOfCascades);

    // Calculate split depths based on view camera frustum
    // Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
    for (uint32_t i = 0; i < settings.nrOfCascades; i++) {
        const float p = (i + 1) / float(settings.nrOfCascades);
        const float log = minZ * std::pow(ratio, p);
        const float uniform = minZ + range * p;
        const float d = settings.cascadeSplitLambda * (log - uniform) + uniform;
        dists[i] = (d - nearClip) / clipRange;
    }

    float lastSplitDist = 0.0;
    for (uint32_t cascade = 0; cascade < settings.nrOfCascades; cascade++) {
        float splitDist = dists[cascade];

        std::array frustumCorners = {
            glm::vec3(-1.0f,  1.0f, -1.0f),
            glm::vec3( 1.0f,  1.0f, -1.0f),
            glm::vec3( 1.0f, -1.0f, -1.0f),
            glm::vec3(-1.0f, -1.0f, -1.0f),
            glm::vec3(-1.0f,  1.0f,  1.0f),
            glm::vec3( 1.0f,  1.0f,  1.0f),
            glm::vec3( 1.0f, -1.0f,  1.0f),
            glm::vec3(-1.0f, -1.0f,  1.0f),
        };

        const glm::mat4 inverse = glm::inverse(viewport.GetCamera().GetProjection() * viewport.GetCamera().GetView());

        for (auto& corner : frustumCorners) {
            glm::vec4 invCorner = inverse * glm::vec4(corner, 1.0f);
            corner = invCorner / invCorner.w;
        }

        for (uint32_t i = 0; i < 4; i++) {
            const glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
            frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
            frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
        }

        glm::vec3 frustumCenter = glm::vec3(0.0f);
        for (const auto& corner : frustumCorners) {
            frustumCenter += corner;
        }
        frustumCenter /= 8.0f;

        glm::vec3 maxExtents = glm::vec3(cascades[cascade].radius);
        glm::vec3 minExtents = -maxExtents;

        glm::vec3 lightDir = glm::normalize(lookDirection);

        glm::mat4 lightViewMatrix = glm::lookAtRH(frustumCenter + lightDir * minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 lightOrthoMatrix = glm::orthoRH_ZO(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

        glm::mat4 shadowMatrix = lightOrthoMatrix * lightViewMatrix;
        glm::vec4 shadowOrigin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        shadowOrigin = shadowMatrix * shadowOrigin;
        shadowOrigin = shadowOrigin * float(settings.resolution) / 2.0f;

        glm::vec4 roundedOrigin = glm::round(shadowOrigin);
        glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
        roundOffset = roundOffset * 2.0f / float(settings.resolution);
        roundOffset.z = 0.0f;
        roundOffset.w = 0.0f;

        glm::mat4 shadowProj = lightOrthoMatrix;
        shadowProj[3] += roundOffset;
        lightOrthoMatrix = shadowProj;

        cascades[cascade].split = (nearClip + splitDist * clipRange) * -1.0f;
        cascades[cascade].matrix = lightOrthoMatrix * lightViewMatrix;

        lastSplitDist = dists[cascade];
    }
}


void ShadowMap::render(const Viewport& viewport, const Scene& scene) {
    glViewport(0, 0, settings.resolution, settings.resolution);

    updateCascades(scene, viewport);

    // setup for rendering
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(settings.depthBiasSlope, settings.depthBiasConstant);
    glDisable(GL_CULL_FACE);

    shader.bind();

    const auto view = scene.view<const Mesh, const Transform>();

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniformBuffer);
    
    for (uint32_t i = 0; i < settings.nrOfCascades; i++) {
        glNamedFramebufferTextureLayer(framebuffer, GL_DEPTH_ATTACHMENT, texture, 0, i);
        glClear(GL_DEPTH_BUFFER_BIT);

        uniforms.lightMatrix = cascades[i].matrix;

        for (const auto& [entity, mesh, transform] : view.each()) {
            uniforms.modelMatrix = transform.worldTransform;

            // determine if we use the original mesh vertices or GPU skinned vertices
            if (scene.all_of<Skeleton>(entity)) {
                glBindBuffer(GL_ARRAY_BUFFER, scene.get<Skeleton>(entity).skinnedVertexBuffer);
            } else {
                glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBuffer);
            }

            BindInterleavedVertexLayout(mesh);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBuffer);

            glNamedBufferSubData(uniformBuffer, 0, sizeof(uniforms), &uniforms);

            glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indices.size(), GL_UNSIGNED_INT, nullptr);
        }
    }

    glDisable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_CULL_FACE);
}


void ShadowMap::renderCascade(const Viewport& viewport, GLuint framebuffer) {
    glViewport(0, 0, viewport.size.x, viewport.size.y);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    debugShader.bind();

    glBindTextureUnit(0, texture);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}


GBuffer::GBuffer(const Viewport& viewport) {
    shader.compile({
        {Shader::Type::VERTEX, "assets\\system\\shaders\\OpenGL\\gbuffer.vert"},
        {Shader::Type::FRAG, "assets\\system\\shaders\\OpenGL\\gbuffer.frag"}
    });

    glCreateBuffers(1, &uniformBuffer);
    glNamedBufferStorage(uniformBuffer, sizeof(uniforms), NULL, GL_DYNAMIC_STORAGE_BIT);

    glCreateBuffers(1, &pbo);
    glNamedBufferStorage(pbo, sizeof(float), NULL, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    entity = glMapNamedBufferRange(pbo, 0, sizeof(float), GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    createRenderTargets(viewport);
}


GBuffer::~GBuffer() {
    glDeleteBuffers(1, &uniformBuffer);
    destroyRenderTargets();
}


void GBuffer::render(const Scene& scene, const Viewport& viewport, uint32_t frameNr) {
    glViewport(0, 0, viewport.size.x, viewport.size.y);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    entt::entity null = entt::null;
    float null_entity_value = float(entt::to_integral(null));
    
    std::array clearColor = { 
        null_entity_value, 0.0f, 0.0f, 1.0f
    };
    
    glClearBufferfv(GL_COLOR, 3, clearColor.data());

    if (frameNr == 0) {
        uniforms.prevJitter = viewport.GetJitter();
    } else {
        uniforms.prevJitter = uniforms.jitter;
    }

    uniforms.jitter = viewport.GetJitter();

    if (frameNr == 0) {
        uniforms.prevViewProj = viewport.GetJitteredProjMatrix() * viewport.GetCamera().GetView();
    } else {
        uniforms.prevViewProj = uniforms.projection * uniforms.view;
    }

    uniforms.view = viewport.GetCamera().GetView();
    uniforms.projection = viewport.GetJitteredProjMatrix();

    Math::Frustrum frustrum;
    frustrum.Create(viewport.GetCamera().GetProjection() * viewport.GetCamera().GetView(), false);

    culled = 0;

    const auto view = scene.view<const Mesh, const Transform>();

    const auto materials = scene.view<const Material>();

    shader.bind();

    for (const auto& [entity, mesh, transform] : view.each()) {
        // convert AABB from local to world space
        std::array<glm::vec3, 2> worldAABB = {
            transform.worldTransform* glm::vec4(mesh.aabb[0], 1.0),
            transform.worldTransform* glm::vec4(mesh.aabb[1], 1.0)
        };

        // if the frustrum can't see the mesh's OBB we cull it
        if (!frustrum.ContainsAABB(worldAABB[0], worldAABB[1])) {
            culled += 1;
            continue;
        }

        const Material* material = nullptr;
        if (scene.valid(mesh.material)) {
            material = scene.try_get<Material>(mesh.material);
        }

        if (material) {
            if (material->gpuAlbedoMap) {
                glBindTextureUnit(1, material->gpuAlbedoMap);
            } else {
                glBindTextureUnit(1, Material::Default.gpuAlbedoMap);
            }

            if (material->gpuNormalMap) {
                glBindTextureUnit(2, material->gpuNormalMap);
            } else {
                glBindTextureUnit(2, Material::Default.gpuNormalMap);
            }

            if (material->gpuMetallicRoughnessMap) {
                glBindTextureUnit(3, material->gpuMetallicRoughnessMap);
            } else {
                glBindTextureUnit(3, Material::Default.gpuMetallicRoughnessMap);
            }

            uniforms.colour = material->albedo;
            uniforms.metallic = material->metallic;
            uniforms.roughness = material->roughness;

        } else {
            glBindTextureUnit(1, Material::Default.gpuAlbedoMap);
            glBindTextureUnit(2, Material::Default.gpuNormalMap);
            glBindTextureUnit(3, Material::Default.gpuMetallicRoughnessMap);
            uniforms.colour = Material::Default.albedo;
            uniforms.metallic = Material::Default.metallic;
            uniforms.roughness = Material::Default.roughness;
        }

        uniforms.model = transform.worldTransform;

        uniforms.entity = entt::to_integral(entity);

        // determine if we use the original mesh vertices or GPU skinned vertices
        if (scene.all_of<Skeleton>(entity)) {
            glBindBuffer(GL_ARRAY_BUFFER, scene.get<Skeleton>(entity).skinnedVertexBuffer);
        } else {
            glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBuffer);
        }

        BindInterleavedVertexLayout(mesh);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBuffer);

        glNamedBufferSubData(uniformBuffer, 0, sizeof(uniforms), &uniforms);

        glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniformBuffer);

        glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indices.size(), GL_UNSIGNED_INT, nullptr);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


uint32_t GBuffer::readEntity(GLint x, GLint y) {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT3);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);

    glReadPixels(x, y, 1, 1, GL_RED, GL_FLOAT, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    GLsync sync =  glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glClientWaitSync(sync, 0, UINT64_MAX);

    return static_cast<uint32_t>(*static_cast<float*>(entity));
}


void GBuffer::createRenderTargets(const Viewport& viewport) {
    glCreateTextures(GL_TEXTURE_2D, 1, &albedoTexture);
    glTextureStorage2D(albedoTexture, 1, GL_RGBA16F, viewport.size.x, viewport.size.y);
    glTextureParameteri(albedoTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(albedoTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateTextures(GL_TEXTURE_2D, 1, &normalTexture);
    glTextureStorage2D(normalTexture, 1, GL_RGBA16F, viewport.size.x, viewport.size.y);
    glTextureParameteri(normalTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(normalTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateTextures(GL_TEXTURE_2D, 1, &materialTexture);
    glTextureStorage2D(materialTexture, 1, GL_RGBA16F, viewport.size.x, viewport.size.y);
    glTextureParameteri(materialTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(materialTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateTextures(GL_TEXTURE_2D, 1, &entityTexture);
    glTextureStorage2D(entityTexture, 1, GL_R32F, viewport.size.x, viewport.size.y);
    glTextureParameteri(entityTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(entityTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateTextures(GL_TEXTURE_2D, 1, &velocityTexture);
    glTextureStorage2D(velocityTexture, 1, GL_RG16F, viewport.size.x, viewport.size.y);
    glTextureParameteri(velocityTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(velocityTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateTextures(GL_TEXTURE_2D, 1, &depthTexture);
    glTextureStorage2D(depthTexture, 1, GL_DEPTH_COMPONENT32F, viewport.size.x, viewport.size.y);
    glTextureParameteri(depthTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(depthTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateFramebuffers(1, &framebuffer);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT0, normalTexture, 0);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT1, albedoTexture, 0);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT2, materialTexture, 0);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT3, entityTexture, 0);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT4, velocityTexture, 0);

    std::array colorAttachments = {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3,
        GL_COLOR_ATTACHMENT4,
    };

    glNamedFramebufferDrawBuffers(
        framebuffer,
        (GLsizei) colorAttachments.size(),
        (GLenum*) colorAttachments.data()
    );

    glNamedFramebufferTexture(framebuffer, GL_DEPTH_ATTACHMENT, depthTexture, 0);
}


void GBuffer::destroyRenderTargets() {
    std::array textures = {
        albedoTexture,
        normalTexture,
        materialTexture,
        depthTexture,
        entityTexture,
        velocityTexture
    };

    glDeleteTextures(GLsizei(textures.size()), textures.data());
    glDeleteFramebuffers(1, &framebuffer);
}


DeferredShading::~DeferredShading() {
    destroyRenderTargets();
    glDeleteBuffers(1, &uniformBuffer);
    glDeleteBuffers(1, &uniformBuffer2);
}



DeferredShading::DeferredShading(const Viewport& viewport) {
    // load shaders from disk
    shader.compile({
        {Shader::Type::VERTEX, "assets\\system\\shaders\\OpenGL\\quad.vert"},
        {Shader::Type::FRAG, "assets\\system\\shaders\\OpenGL\\pbr.frag"}
    });

    brdfLUTshader.compile({
        {Shader::Type::VERTEX, "assets\\system\\shaders\\OpenGL\\quad.vert"},
        {Shader::Type::FRAG, "assets\\system\\shaders\\OpenGL\\brdfLUT.frag"}
    });

    // init resources
    createRenderTargets(viewport);

    // init uniform buffer
    glCreateBuffers(1, &uniformBuffer);
    glNamedBufferStorage(uniformBuffer, sizeof(uniforms), NULL, GL_DYNAMIC_STORAGE_BIT);

    glCreateBuffers(1, &uniformBuffer2);
    glNamedBufferStorage(uniformBuffer2, sizeof(uniforms2), NULL, GL_DYNAMIC_STORAGE_BIT);

    glCreateTextures(GL_TEXTURE_2D, 1, &brdfLUT);
    glTextureStorage2D(brdfLUT, 1, GL_RG16F, 512, 512);
    glTextureParameteri(brdfLUT, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(brdfLUT, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(brdfLUT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(brdfLUT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glCreateFramebuffers(1, &brdfLUTframebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, brdfLUTframebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUT, 0);

    glViewport(0, 0, 512, 512);
    glClear(GL_COLOR_BUFFER_BIT);

    brdfLUTshader.bind();

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}



void DeferredShading::render(const Scene& sscene, const Viewport& viewport,
                             const ShadowMap& shadowMap, const GBuffer& GBuffer, const Atmosphere& atmosphere, const Voxelize& voxels) {
    uniforms.view = viewport.GetCamera().GetView();
    uniforms.projection = viewport.GetJitteredProjMatrix();

    // TODO: figure out this directional light crap, I only really want to support a single one or figure out a better way to deal with this
    // For now we send only the first directional light to the GPU for everything, if none are present we send a buffer with a direction of (0, -1, 0)
    {
        const auto view = sscene.view<const DirectionalLight, const Transform>();
        auto entity = view.front();
        if (entity != entt::null) {
            const auto& light = view.get<const DirectionalLight>(entity);
            const auto& transform = view.get<const Transform>(entity);
            uniforms.dirLight = light;
        } else {
            auto light = DirectionalLight();
            light.direction.y = -0.9f;
            uniforms.dirLight = light;
        }
    }

    const auto posView = sscene.view<const PointLight, const Transform>();

    unsigned int i = 0;
    for (auto entity : posView) {
        const auto& light = posView.get<const PointLight>(entity);
        const auto& transform = posView.get<const Transform>(entity);

        i++;

        if (i >= ARRAYSIZE(uniforms.pointLights)) {
            break;
        }

        uniforms.pointLights[i] = light;
    }

    uniforms.cameraPosition = glm::vec4(viewport.GetCamera().GetPosition(), 1.0);
    
    for (uint32_t cascade = 0; cascade < shadowMap.settings.nrOfCascades; cascade++) {
        uniforms.shadowMatrices[cascade] = shadowMap.cascades[cascade].matrix;
        uniforms.shadowSplits[cascade] = shadowMap.cascades[cascade].split;
    }

    uniforms2.bloomThreshold = glm::vec4(settings.bloomThreshold, 0.0f);
    uniforms2.pointLightCount = static_cast<uint32_t>(sscene.size<PointLight>());
    uniforms2.directionalLightCount = static_cast<uint32_t>(sscene.size<DirectionalLight>());;
    uniforms2.voxelWorldSize = voxels.worldSize;
    uniforms2.invViewProjection = glm::inverse(uniforms.projection * uniforms.view);

    glNamedBufferSubData(uniformBuffer, 0, sizeof(uniforms), &uniforms);
    glNamedBufferSubData(uniformBuffer2, 0, sizeof(uniforms2), &uniforms2);

    glViewport(0, 0, viewport.size.x, viewport.size.y);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glClear(GL_COLOR_BUFFER_BIT);

    shader.bind();

    glBindTextureUnit(0, shadowMap.texture);
    glBindTextureUnit(3, GBuffer.albedoTexture);
    glBindTextureUnit(4, GBuffer.normalTexture);
    glBindTextureUnit(6, voxels.result);
    glBindTextureUnit(7, GBuffer.materialTexture);
    glBindTextureUnit(8, GBuffer.depthTexture);
    glBindTextureUnit(9, brdfLUT);
    glBindTextureUnit(10, atmosphere.environmentCubemap);
    glBindTextureUnit(11, atmosphere.convolvedCubemap);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniformBuffer);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, uniformBuffer2);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}



void DeferredShading::createRenderTargets(const Viewport& viewport) {
    // init render targets
    glCreateTextures(GL_TEXTURE_2D, 1, &result);
    glTextureStorage2D(result, 1, GL_RGBA16F, viewport.size.x, viewport.size.y);
    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateTextures(GL_TEXTURE_2D, 1, &bloomHighlights);
    glTextureStorage2D(bloomHighlights, 1, GL_RGBA16F, viewport.size.x, viewport.size.y);
    glTextureParameteri(bloomHighlights, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(bloomHighlights, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(bloomHighlights, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(bloomHighlights, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(bloomHighlights, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glCreateFramebuffers(1, &framebuffer);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT0, result, 0);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT1, bloomHighlights, 0);

    std::array colorAttachments = {
        GLenum(GL_COLOR_ATTACHMENT0), GLenum(GL_COLOR_ATTACHMENT1)
    };

    glNamedFramebufferDrawBuffers(framebuffer, static_cast<GLsizei>(colorAttachments.size()), colorAttachments.data());
}

void DeferredShading::destroyRenderTargets() {
    glDeleteTextures(1, &result);
    glDeleteTextures(1, &bloomHighlights);
    glDeleteFramebuffers(1, &framebuffer);
}



Bloom::~Bloom() {
    destroyRenderTargets();
    glDeleteBuffers(1, &uniformBuffer);
}



Bloom::Bloom(const Viewport& viewport) {
    blurShader.compile({
        {Shader::Type::VERTEX, "assets\\system\\shaders\\OpenGL\\quad.vert"},
        {Shader::Type::FRAG, "assets\\system\\shaders\\OpenGL\\gaussian.frag"}
    });

    createRenderTargets(viewport);

    glCreateBuffers(1, &uniformBuffer);
    glNamedBufferStorage(uniformBuffer, sizeof(uniforms), NULL, GL_DYNAMIC_STORAGE_BIT);
}



void Bloom::render(const Viewport& viewport, GLuint highlights) {
    if (viewport.size.x < 16.0f || viewport.size.y < 16.0f) {
        return;
    }

    auto quarter = glm::ivec2(viewport.size.x / 4, viewport.size.y / 4);

    glNamedFramebufferTexture(highlightsFramebuffer, GL_COLOR_ATTACHMENT0, highlights, 0);

    glBlitNamedFramebuffer(highlightsFramebuffer, bloomFramebuffer,
                           0, 0, viewport.size.x, viewport.size.y,
                           0, 0, quarter.x, quarter.y,
                           GL_COLOR_BUFFER_BIT, GL_LINEAR);

    // horizontally blur 1/4th bloom to blur texture
    blurShader.bind();
    glViewport(0, 0, quarter.x, quarter.y);
    glBindFramebuffer(GL_FRAMEBUFFER, blurFramebuffer);
    glClear(GL_COLOR_BUFFER_BIT);

    uniforms.direction = glm::vec2(1, 0);
    glNamedBufferSubData(uniformBuffer, 0, sizeof(uniforms), &uniforms);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniformBuffer);

    glBindTextureUnit(0, bloomTexture);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // vertically blur the blur to bloom texture
    glBindFramebuffer(GL_FRAMEBUFFER, bloomFramebuffer);
    glClear(GL_COLOR_BUFFER_BIT);

    uniforms.direction = glm::vec2(0, 1);
    glNamedBufferSubData(uniformBuffer, 0, sizeof(uniforms), &uniforms);

    glBindTextureUnit(0, blurTexture);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glViewport(0, 0, viewport.size.x, viewport.size.y);
}



void Bloom::createRenderTargets(const Viewport& viewport) {
    auto quarterRes = glm::ivec2(
                          std::max(viewport.size.x / 4, 1u),
                          std::max(viewport.size.y / 4, 1u)
                      );

    glCreateTextures(GL_TEXTURE_2D, 1, &bloomTexture);
    glTextureStorage2D(bloomTexture, 1, GL_RGBA16F, quarterRes.x, quarterRes.y);
    glTextureParameteri(bloomTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(bloomTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(bloomTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(bloomTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(bloomTexture, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glCreateTextures(GL_TEXTURE_2D, 1, &blurTexture);
    glTextureStorage2D(blurTexture, 1, GL_RGBA16F, quarterRes.x, quarterRes.y);
    glTextureParameteri(blurTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(blurTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(blurTexture, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTextureParameteri(blurTexture, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTextureParameteri(blurTexture, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT);

    glCreateFramebuffers(1, &bloomFramebuffer);
    glNamedFramebufferTexture(bloomFramebuffer, GL_COLOR_ATTACHMENT0, bloomTexture, 0);
    glNamedFramebufferDrawBuffer(bloomFramebuffer, GL_COLOR_ATTACHMENT0);

    glCreateFramebuffers(1, &blurFramebuffer);
    glNamedFramebufferTexture(blurFramebuffer, GL_COLOR_ATTACHMENT0, blurTexture, 0);
    glNamedFramebufferDrawBuffer(blurFramebuffer, GL_COLOR_ATTACHMENT0);

    glCreateFramebuffers(1, &highlightsFramebuffer);
    glNamedFramebufferDrawBuffer(highlightsFramebuffer, GL_COLOR_ATTACHMENT0);
}



void Bloom::destroyRenderTargets() {
    glDeleteFramebuffers(1, &bloomFramebuffer);
    glDeleteFramebuffers(1, &blurFramebuffer);
    glDeleteTextures(1, &bloomTexture);
    glDeleteTextures(1, &blurTexture);
}



Tonemap::~Tonemap() {
    destroyRenderTargets();
}



Tonemap::Tonemap(const Viewport& viewport) {
    // load shaders from disk
    shader.compile({
        {Shader::Type::VERTEX, "assets\\system\\shaders\\OpenGL\\quad.vert"},
        {Shader::Type::FRAG, "assets\\system\\shaders\\OpenGL\\tonemap.frag"}
    });

    // init render targets
    createRenderTargets(viewport);

    // init uniform buffer
    glCreateBuffers(1, &uniformBuffer);
    glNamedBufferStorage(uniformBuffer, sizeof(settings), NULL, GL_DYNAMIC_STORAGE_BIT);
}



void Tonemap::render(GLuint scene, GLuint bloom) {
    // bind and clear render target
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // bind shader and input texture
    shader.bind();
    glBindTextureUnit(0, scene);
    glBindTextureUnit(1, bloom);

    // update uniform buffer GPU side
    glNamedBufferSubData(uniformBuffer, 0, sizeof(settings), &settings);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniformBuffer);

    // render fullscreen quad to perform tonemapping
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}



void Tonemap::createRenderTargets(const Viewport& viewport) {
    // init render targets
    glCreateTextures(GL_TEXTURE_2D, 1, &result);
    glTextureStorage2D(result, 1, GL_RGB8, viewport.size.x, viewport.size.y);
    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateFramebuffers(1, &framebuffer);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT0, result, 0);
    glNamedFramebufferDrawBuffer(framebuffer, GL_COLOR_ATTACHMENT0);
}



void Tonemap::destroyRenderTargets() {
    glDeleteTextures(1, &result);
    glDeleteFramebuffers(1, &framebuffer);
}



Voxelize::Voxelize(uint32_t size) : size(size) {
    // load shaders from disk
    shader.compile({
        {Shader::Type::VERTEX, "assets\\system\\shaders\\OpenGL\\voxelize.vert"},
        {Shader::Type::GEO, "assets\\system\\shaders\\OpenGL\\voxelize.geom"},
        {Shader::Type::FRAG, "assets\\system\\shaders\\OpenGL\\voxelize.frag"}
    });

    mipmapShader.compile({ {Shader::Type::COMPUTE, "assets\\system\\shaders\\OpenGL\\mipmap.comp"} });
    opacityFixShader.compile({ {Shader::Type::COMPUTE, "assets\\system\\shaders\\OpenGL\\correctAlpha.comp"} });

    glCreateBuffers(1, &uniformBuffer);
    glNamedBufferStorage(uniformBuffer, sizeof(uniforms), NULL, GL_DYNAMIC_STORAGE_BIT);

    glCreateTextures(GL_TEXTURE_3D, 1, &result);
    glTextureStorage3D(result, static_cast<GLsizei>(std::log2(size)), GL_RGBA8, size, size, size);
    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(result, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(result, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTextureParameteri(result, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    glTextureParameterfv(result, GL_TEXTURE_BORDER_COLOR, borderColor);
    glGenerateTextureMipmap(result);
}



void Voxelize::computeMipmaps(GLuint texture) {
    int level = 0, texSize = size;
    while (texSize >= 1.0f) {
        texSize = static_cast<int>(texSize * 0.5f);
        mipmapShader.bind();
        glBindImageTexture(0, texture, level, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA8);
        glBindImageTexture(1, texture, level + 1, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
        // TODO: query local work group size at startup
        glDispatchCompute(static_cast<GLuint>(size / 64), texSize, texSize);
        mipmapShader.unbind();
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        level++;
    }
}



void Voxelize::correctOpacity(GLuint texture) {
    opacityFixShader.bind();
    glBindImageTexture(0, texture, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
    // local work group size is 64
    glDispatchCompute(static_cast<GLuint>(size / 64), size, size);
    opacityFixShader.unbind();
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}



void Voxelize::render(const Scene& scene, const Viewport& viewport, const ShadowMap& shadowmap) {
    // left, right, bottom, top, zNear, zFar
    auto projectionMatrix = glm::ortho(-worldSize * 0.5f, worldSize * 0.5f, -worldSize * 0.5f, worldSize * 0.5f, worldSize * 0.5f, worldSize * 1.5f);
    uniforms.px = projectionMatrix * glm::lookAt(glm::vec3(worldSize, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    uniforms.py = projectionMatrix * glm::lookAt(glm::vec3(0, worldSize, 0), glm::vec3(0, 0, 0), glm::vec3(0, 0, -1));
    uniforms.pz = projectionMatrix * glm::lookAt(glm::vec3(0, 0, worldSize), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

    for (uint32_t cascade = 0; cascade < shadowmap.settings.nrOfCascades; cascade++) {
        uniforms.shadowMatrices[cascade] = shadowmap.cascades[cascade].matrix;
        uniforms.shadowSplits[cascade] = shadowmap.cascades[cascade].split;
    }

    uniforms.view = viewport.GetCamera().GetView();

    // clear the entire voxel texture
    constexpr auto clearColour = glm::u8vec4(0.0f, 0.0f, 0.0f, 0.0f);
    for (uint32_t level = 0; level < std::log2(size); level++) {
        //glClearTexImage(result, level, GL_RGBA, GL_UNSIGNED_BYTE, glm::value_ptr(clearColour));
    }

    // set GL state
    glViewport(0, 0, size, size);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    //glEnable(GL_CONSERVATIVE_RASTERIZATION_NV);

    // bind shader and level 0 of the voxel volume
    shader.bind();
    glBindImageTexture(1, result, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
    glBindTextureUnit(2, shadowmap.texture);

    const auto view = scene.view<const Mesh, const Transform>();

    for (const auto& [entity, mesh, transform] : view.each()) {

        const Material* material = nullptr;
        
        if (scene.valid(mesh.material)) {
            material = scene.try_get<Material>(mesh.material);
        }

        uniforms.model = transform.worldTransform;

        if (material) {
            if (material->gpuAlbedoMap) {
                glBindTextureUnit(0, material->gpuAlbedoMap);
            } else {
                glBindTextureUnit(0, Material::Default.gpuAlbedoMap);
            }
            uniforms.colour = material->albedo;
        } else {
            glBindTextureUnit(0, Material::Default.gpuAlbedoMap);
            uniforms.colour = Material::Default.albedo;
        }

        // determine if we use the original mesh vertices or GPU skinned vertices
        if (scene.all_of<Skeleton>(entity)) {
            glBindBuffer(GL_ARRAY_BUFFER, scene.get<Skeleton>(entity).skinnedVertexBuffer);
        } else {
            glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBuffer);
        }

        BindInterleavedVertexLayout(mesh);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBuffer);

        glNamedBufferSubData(uniformBuffer, 0, sizeof(uniforms), &uniforms);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniformBuffer);

        glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indices.size(), GL_UNSIGNED_INT, nullptr);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // Run compute shaders
    computeMipmaps(result);
    //correctOpacity(result);
    //glGenerateTextureMipmap(result);

    // reset OpenGL state
    glViewport(0, 0, viewport.size.x, viewport.size.y);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    //glDisable(GL_CONSERVATIVE_RASTERIZATION_NV);

}



VoxelizeDebug::~VoxelizeDebug() {
    destroyRenderTargets();
    glDeleteBuffers(1, &uniformBuffer);
}



VoxelizeDebug::VoxelizeDebug(const Viewport& viewport) {
    shader.compile({
        {Shader::Type::VERTEX, "assets\\system\\shaders\\OpenGL\\voxelDebug.vert"},
        {Shader::Type::GEO, "assets\\system\\shaders\\OpenGL\\voxelDebug.geom"},
        {Shader::Type::FRAG, "assets\\system\\shaders\\OpenGL\\voxelDebug.frag"}
    });

    createRenderTargets(viewport);

    glCreateBuffers(1, &uniformBuffer);
    glNamedBufferStorage(uniformBuffer, sizeof(uniforms), NULL, GL_DYNAMIC_STORAGE_BIT);
}



VoxelizeDebug::VoxelizeDebug(const Viewport& viewport, uint32_t voxelTextureSize) {
    shader.compile({
        {Shader::Type::VERTEX, "assets\\system\\shaders\\OpenGL\\voxelDebugFast.vert"},
        {Shader::Type::FRAG, "assets\\system\\shaders\\OpenGL\\voxelDebugFast.frag"}
    });

    // init resources
    createRenderTargets(viewport);

    glCreateBuffers(1, &uniformBuffer);
    glNamedBufferStorage(uniformBuffer, sizeof(uniforms), NULL, GL_DYNAMIC_STORAGE_BIT);

    const bool CUBE_BACKFACE_OPTIMIZATION = true;

    constexpr size_t NUM_CUBE_INDICES = CUBE_BACKFACE_OPTIMIZATION ? 3 * 3 * 2 : 3 * 6 * 2;
    constexpr size_t NUM_CUBE_VERTICES = 8;

    constexpr std::array<uint32_t, 36> cubeIndices = {
        0, 2, 1, 2, 3, 1,
        5, 4, 1, 1, 4, 0,
        0, 4, 6, 0, 6, 2,
        6, 5, 7, 6, 4, 5,
        2, 6, 3, 6, 7, 3,
        7, 1, 3, 7, 5, 1
    };

    const size_t numIndices = static_cast<size_t>(std::pow(voxelTextureSize, 3u) * NUM_CUBE_INDICES);

    std::vector<uint32_t> indexBufferData(numIndices);

    const auto chunks = { 1, 2, 3, 4, 5, 6, 7, 8 };
    const size_t CHUNK_SIZE = numIndices / chunks.size();


    std::for_each(std::execution::par_unseq, chunks.begin(), chunks.end(), [&](const int chunk) {
        const size_t start = (chunk - 1) * CHUNK_SIZE;
        const size_t end = chunk * CHUNK_SIZE;

        for (size_t i = start; i < end; i++) {
            auto cube = i / NUM_CUBE_INDICES;
            auto cube_local = i % NUM_CUBE_INDICES;
            indexBufferData[i] = static_cast<uint32_t>(cubeIndices[cube_local] + cube * NUM_CUBE_VERTICES);
        }
    });

    glCreateBuffers(1, &indexBuffer);
    glNamedBufferData(indexBuffer, sizeof(uint32_t) * indexBufferData.size(), indexBufferData.data(), GL_STATIC_DRAW);
    indexCount = static_cast<uint32_t>(indexBufferData.size());
}

void VoxelizeDebug::render(const Viewport& viewport, GLuint input, const Voxelize& voxels) {
    // bind the input framebuffer, we draw the debug vertices on top
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
    glNamedFramebufferTexture(frameBuffer, GL_COLOR_ATTACHMENT0, input, 0);
    glNamedFramebufferDrawBuffer(frameBuffer, GL_COLOR_ATTACHMENT0);
    glClear(GL_DEPTH_BUFFER_BIT);

    float voxelSize = voxels.worldSize / voxels.size;
    glm::mat4 modelMatrix = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(voxelSize)), glm::vec3(0, 0, 0));

    // bind shader and set uniforms
    shader.bind();
    uniforms.p = viewport.GetCamera().GetProjection();
    uniforms.mv = viewport.GetCamera().GetView() * modelMatrix;

    glNamedBufferSubData(uniformBuffer, 0, sizeof(uniforms), &uniforms);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniformBuffer);

    glBindTextureUnit(0, voxels.result);

    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(std::pow(voxels.size, 3)));

    // unbind framebuffers
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}



void VoxelizeDebug::execute2(const Viewport& viewport, GLuint input, const Voxelize& voxels) {
    // bind the input framebuffer, we draw the debug vertices on top
    glDisable(GL_CULL_FACE);

    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
    glNamedFramebufferTexture(frameBuffer, GL_COLOR_ATTACHMENT0, input, 0);
    glNamedFramebufferDrawBuffer(frameBuffer, GL_COLOR_ATTACHMENT0);
    glClear(GL_DEPTH_BUFFER_BIT);

    float voxelSize = voxels.worldSize / voxels.size;
    glm::mat4 modelMatrix = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(voxelSize)), glm::vec3(0, 0, 0));

    // bind shader and set uniforms
    shader.bind();
    uniforms.p = viewport.GetCamera().GetProjection();
    uniforms.mv = viewport.GetCamera().GetView() * modelMatrix;
    uniforms.cameraPosition = glm::vec4(viewport.GetCamera().GetPosition(), 1.0);

    glNamedBufferSubData(uniformBuffer, 0, sizeof(uniforms), &uniforms);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniformBuffer);

    glBindTextureUnit(0, voxels.result);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);

    // unbind framebuffers
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glEnable(GL_CULL_FACE);
}



void VoxelizeDebug::createRenderTargets(const Viewport& viewport) {
    glCreateRenderbuffers(1, &renderBuffer);
    glNamedRenderbufferStorage(renderBuffer, GL_DEPTH32F_STENCIL8, viewport.size.x, viewport.size.y);

    glCreateFramebuffers(1, &frameBuffer);
    glNamedFramebufferRenderbuffer(frameBuffer, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderBuffer);
}



void VoxelizeDebug::destroyRenderTargets() {
    glDeleteRenderbuffers(1, &renderBuffer);
    glDeleteFramebuffers(1, &frameBuffer);
}



DebugLines::~DebugLines() {
    glDeleteFramebuffers(1, &frameBuffer);
    glDeleteBuffers(1, &uniformBuffer);
}



DebugLines::DebugLines() {
    shader.compile({
        {Shader::Type::VERTEX, "assets\\system\\shaders\\OpenGL\\aabb.vert"},
        {Shader::Type::FRAG, "assets\\system\\shaders\\OpenGL\\aabb.frag"}
    });

    glCreateFramebuffers(1, &frameBuffer);

    glCreateBuffers(1, &uniformBuffer);
    glNamedBufferStorage(uniformBuffer, sizeof(uniforms), NULL, GL_DYNAMIC_STORAGE_BIT);
}



void DebugLines::render(const Viewport& viewport, GLuint colorAttachment, GLuint depthAttachment) {
    if (points.size() < 2) return;

    glEnable(GL_LINE_SMOOTH);

    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
    glNamedFramebufferTexture(frameBuffer, GL_COLOR_ATTACHMENT0, colorAttachment, 0);
    glNamedFramebufferTexture(frameBuffer, GL_DEPTH_ATTACHMENT, depthAttachment, 0);
    glNamedFramebufferDrawBuffer(frameBuffer, GL_COLOR_ATTACHMENT0);

    shader.bind();
    uniforms.projection = viewport.GetCamera().GetProjection();
    uniforms.view = viewport.GetCamera().GetView();

    glNamedBufferSubData(uniformBuffer, 0, sizeof(uniforms), &uniforms);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniformBuffer);

    if (vertexBuffer) glDeleteBuffers(1, &vertexBuffer);
    glCreateBuffers(1, &vertexBuffer);
    glNamedBufferData(vertexBuffer, sizeof(float) * points.size() * 3, glm::value_ptr(points[0]), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (const void*)((intptr_t)0));

    glDrawArrays(GL_LINES, 0, (GLsizei)points.size());

    glDisable(GL_LINE_SMOOTH);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    points.clear();
}



Skinning::Skinning() {
    computeShader.compile({ {Shader::Type::COMPUTE, "assets\\system\\shaders\\OpenGL\\skinning.comp"} });
}



void Skinning::compute(const Mesh& mesh, const Skeleton& anim) {

    glNamedBufferData(anim.boneTransformsBuffer, anim.m_BoneTransforms.size() * sizeof(glm::mat4), anim.m_BoneTransforms.data(), GL_DYNAMIC_DRAW);

    computeShader.bind();

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, anim.boneIndexBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, anim.boneWeightBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, mesh.vertexBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, anim.skinnedVertexBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, anim.boneTransformsBuffer);

    glDispatchCompute(static_cast<GLuint>(mesh.positions.size()), 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}



inline double random_double() {
    static std::uniform_real_distribution<double> distribution(0.0, 1.0);
    static std::mt19937 generator;
    return distribution(generator);
}



inline double random_double(double min, double max) {
    static std::uniform_real_distribution<double> distribution(min, max);
    static std::mt19937 generator;
    return distribution(generator);
}



inline glm::vec3 random_color() {
    return glm::vec3(random_double(), random_double(), random_double());
}



inline glm::vec3 random_color(double min, double max) {
    return glm::vec3(random_double(min, max), random_double(min, max), random_double(min, max));
}



RayTracingOneWeekend::RayTracingOneWeekend(const Viewport& viewport) {
    shader.compile({ {Shader::Type::COMPUTE, "assets\\system\\shaders\\OpenGL\\ray.comp"} });

    spheres.push_back(Sphere{ glm::vec3(0, -1000, 0), glm::vec3(0.5, 0.5, 0.5), 1.0f, 0.0f, 1000.0f });

    int count = 3;
    for (int a = -count; a < count; a++) {
        for (int b = -count; b < count; b++) {
            auto choose_mat = random_double();
            glm::vec3 center(a + 0.9 * random_double(), 0.2, b + 0.9 * random_double());

            if ((center - glm::vec3(4, 0.2, 0)).length() > 0.9) {
                Sphere sphere;

                if (choose_mat < 0.8) {
                    // diffuse
                    sphere.colour = random_color() * random_color();
                    sphere.metalness = 0.0f;
                    sphere.radius = 0.2f;
                    sphere.origin = center;
                    spheres.push_back(sphere);
                } else if (choose_mat < 0.95) {
                    // metal
                    sphere.colour = random_color(0.5, 1);
                    sphere.roughness = static_cast<float>(random_double(0, 0.5));
                    sphere.metalness = 1.0f;
                    sphere.radius = 0.2f;
                    sphere.origin = center;
                    spheres.push_back(sphere);
                }
            }
        }
    }

    spheres.push_back(Sphere{ glm::vec3(-4, 1, 0), glm::vec3(0.4, 0.2, 0.1), 1.0f, 0.0f, 1.0f });
    spheres.push_back(Sphere{ glm::vec3(4, 1, 0), glm::vec3(0.7, 0.6, 0.5), 0.0f, 1.0f, 1.0f });

    createRenderTargets(viewport);
    auto clearColour = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    glClearTexImage(result, 0, GL_RGBA, GL_FLOAT, glm::value_ptr(clearColour));

    glCreateBuffers(1, &uniformBuffer);
    glNamedBufferStorage(uniformBuffer, sizeof(uniforms), NULL, GL_DYNAMIC_STORAGE_BIT);
}



RayTracingOneWeekend::~RayTracingOneWeekend() {
    destroyRenderTargets();
    glDeleteBuffers(1, &sphereBuffer);
    glDeleteBuffers(1, &uniformBuffer);

}



void RayTracingOneWeekend::compute(const Viewport& viewport, bool update) {
    // if the shader changed or we moved the camera we clear the result
    if (!update) {
        glDeleteBuffers(1, &sphereBuffer);
        glCreateBuffers(1, &sphereBuffer);
        glNamedBufferStorage(sphereBuffer, spheres.size() * sizeof(Sphere), spheres.data(), GL_DYNAMIC_STORAGE_BIT);
    }

    shader.bind();
    glBindImageTexture(0, result, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
    glBindImageTexture(1, finalResult, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, sphereBuffer);

    uniforms.iTime = rayTimer.GetElapsedTime();
    uniforms.position = glm::vec4(viewport.GetCamera().GetPosition(), 1.0);
    uniforms.projection = viewport.GetCamera().GetProjection();
    uniforms.view = viewport.GetCamera().GetView();
    uniforms.doUpdate = update;

    const GLuint numberOfSpheres = static_cast<GLuint>(spheres.size());
    uniforms.sphereCount = numberOfSpheres;

    glNamedBufferSubData(uniformBuffer, 0, sizeof(uniforms), &uniforms);
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, uniformBuffer);

    glDispatchCompute(viewport.size.x / 16, viewport.size.y / 16, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}



void RayTracingOneWeekend::createRenderTargets(const Viewport& viewport) {
    glCreateTextures(GL_TEXTURE_2D, 1, &result);
    glTextureStorage2D(result, 1, GL_RGBA32F, viewport.size.x, viewport.size.y);
    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateTextures(GL_TEXTURE_2D, 1, &finalResult);
    glTextureStorage2D(finalResult, 1, GL_RGBA32F, viewport.size.x, viewport.size.y);
    glTextureParameteri(finalResult, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(finalResult, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCreateBuffers(1, &sphereBuffer);
    glNamedBufferStorage(sphereBuffer, spheres.size() * sizeof(Sphere), spheres.data(), GL_DYNAMIC_STORAGE_BIT);
}



void RayTracingOneWeekend::destroyRenderTargets() {
    glDeleteTextures(1, &result);
    glDeleteTextures(1, &finalResult);
}



Icons::Icons(const Viewport& viewport) {
    shader.compile({
        {Shader::Type::VERTEX, "assets\\system\\shaders\\OpenGL\\billboard.vert"},
        {Shader::Type::FRAG, "assets\\system\\shaders\\OpenGL\\billboard.frag"}
    });

    int w, h, ch;
    stbi_set_flip_vertically_on_load(true);
    
    auto img = stbi_load("assets/system/light.png", &w, &h, &ch, 4);
    assert(img);

    glCreateTextures(GL_TEXTURE_2D, 1, &lightTexture);
    glTextureStorage2D(lightTexture, 1, GL_RGBA8, w, h);
    glTextureSubImage2D(lightTexture, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glTextureParameteri(lightTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(lightTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(lightTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(lightTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(lightTexture, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glGenerateTextureMipmap(lightTexture);
    createRenderTargets(viewport);

    glCreateBuffers(1, &uniformBuffer);
    glNamedBufferStorage(uniformBuffer, sizeof(uniforms), NULL, GL_DYNAMIC_STORAGE_BIT);
}

Icons::~Icons() {
    destroyRenderTargets();
    glDeleteBuffers(1, &uniformBuffer);

}



void Icons::createRenderTargets(const Viewport& viewport) {
    glCreateFramebuffers(1, &framebuffer);
}



void Icons::destroyRenderTargets() {
    glDeleteFramebuffers(1, &framebuffer);
}



void Icons::render(const Scene& scene, const Viewport& viewport, GLuint colorAttachment, GLuint entityAttachment) {
    glDisable(GL_CULL_FACE);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT0, colorAttachment, 0);
    glNamedFramebufferTexture(framebuffer, GL_COLOR_ATTACHMENT1, entityAttachment, 0);

    GLenum attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glNamedFramebufferDrawBuffers(framebuffer, 2, attachments);

    glBindTextureUnit(0, lightTexture);

    shader.bind();

    const auto vp = viewport.GetCamera().GetProjection() * viewport.GetCamera().GetView();

    const auto view = scene.view<const DirectionalLight, const Transform>();

    for (const auto& [entity, light, transform] : view.each()) {

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, transform.position);

        glm::vec3 V;
        V.x = viewport.GetCamera().GetPosition().x - transform.position.x;
        V.y = 0;
        V.z = viewport.GetCamera().GetPosition().z - transform.position.z;

        auto Vnorm = glm::normalize(V);

        glm::vec3 lookAt = glm::vec3(0.0f, 0.0f, 1.0f);

        auto upAux = glm::cross(lookAt, Vnorm);
        auto cosTheta = glm::dot(lookAt, Vnorm);

        model = glm::rotate(model, acos(cosTheta), upAux);

        model = glm::scale(model, glm::vec3(glm::length(V) * 0.05f));

        uniforms.mvp = vp * model;
        uniforms.entity = entt::to_integral(entity);
        uniforms.world_position = glm::vec4(transform.position, 1.0);

        glNamedBufferSubData(uniformBuffer, 0, sizeof(uniforms), &uniforms);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniformBuffer);

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glEnable(GL_CULL_FACE);
}



Atmosphere::Atmosphere(const Viewport& viewport) {
    convoluteShader.compile({
        { Shader::Type::COMPUTE, "assets\\system\\shaders\\OpenGL\\convolute.comp" },
    });


    computeShader.compile({
        { Shader::Type::COMPUTE, "assets\\system\\shaders\\OpenGL\\atmosphere.comp" },
    });

    createRenderTargets(viewport);

    glCreateBuffers(1, &uniformBuffer);
    glNamedBufferStorage(uniformBuffer, sizeof(uniforms), NULL, GL_DYNAMIC_STORAGE_BIT);

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &convolvedCubemap);
    glTextureStorage2D(convolvedCubemap, 1, GL_RGBA16F, 32, 32);
    glTextureParameteri(convolvedCubemap, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(convolvedCubemap, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(convolvedCubemap, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTextureParameteri(convolvedCubemap, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(convolvedCubemap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &environmentCubemap);
    glTextureStorage2D(environmentCubemap, 1, GL_RGBA16F, 512, 512);
    glTextureParameteri(environmentCubemap, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(environmentCubemap, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(environmentCubemap, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTextureParameteri(environmentCubemap, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(environmentCubemap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}



Atmosphere::~Atmosphere() {
    destroyRenderTargets();
    glDeleteBuffers(1, &uniformBuffer);
}



void Atmosphere::createRenderTargets(const Viewport& viewport) {
    glCreateFramebuffers(1, &framebuffer);
}



void Atmosphere::destroyRenderTargets() {
    glDeleteFramebuffers(1, &framebuffer);
}



void Atmosphere::computeCubemaps(const Viewport& viewport, const Scene& scene) {
    uniforms.view = glm::mat3(viewport.GetCamera().GetView());
    uniforms.proj = viewport.GetCamera().GetProjection();

    auto light = DirectionalLight();
    light.direction.y = -0.9f;

    uniforms.invViewProj = glm::inverse(viewport.GetCamera().GetProjection() * viewport.GetCamera().GetView());
    uniforms.cameraPos = glm::vec4(viewport.GetCamera().GetPosition(), 1.0);

    // TODO: figure out this directional light crap, I only really want to support a single one or figure out a better way to deal with this
    // For now we send only the first directional light to the GPU for everything, if none are present we send a buffer with a direction of (0, -0.9, 0)
    {
        const auto view = scene.view<const DirectionalLight, const Transform>();
        const auto entity = view.front();
        if (entity != entt::null) {
            const auto& light = view.get<const DirectionalLight>(entity);
            const auto& transform = view.get<const Transform>(entity);

            uniforms.sunlightDir = light.direction;
            uniforms.sunlightColor = light.colour;
        } else {
            auto light = DirectionalLight();
            light.direction.y = -0.9f;
            uniforms.sunlightDir = light.direction;
            uniforms.sunlightColor = light.colour;
        }
    }

    glNamedBufferSubData(uniformBuffer, 0, sizeof(uniforms), &uniforms);

    computeShader.bind();
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniformBuffer);
    glBindImageTexture(0, environmentCubemap, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glDispatchCompute(512 / 8, 512 / 8, 6);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    //convoluteShader.bind();
    //glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniformBuffer);
    //glBindTextureUnit(0, environmentCubemap);
    //glBindImageTexture(1, convolvedCubemap, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    //glDispatchCompute(32 / 8, 32 / 8, 6);

    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
}



TAAResolve::TAAResolve(const Viewport& viewport) {
    shader.compile({ { Shader::Type::COMPUTE, "assets\\system\\shaders\\OpenGL\\taaResolve.comp" } });

    createRenderTargets(viewport);
}

GLuint TAAResolve::render(const Viewport& viewport, const GBuffer& gbuffer, const DeferredShading& shading, uint32_t frameNr) {
    shader.bind();

    // on first frame it should bind the hdr shading result as history texture
    if (frameNr == 0) {
        glBindTextureUnit(0, shading.result);
        glBindImageTexture(5, shading.result, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);

    }
    else {
        glBindTextureUnit(0, historyBuffer);
        glBindImageTexture(5, historyBuffer, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
    }

    glBindImageTexture(1, resultBuffer, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glBindTextureUnit(2, gbuffer.depthTexture);
    glBindImageTexture(3, gbuffer.velocityTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG16F);
    glBindImageTexture(4, shading.result, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);

    auto dispatch = [](uint32_t size) -> GLuint {
        return GLuint(std::ceil(size / 8.0f));
    };

    glDispatchCompute(dispatch(viewport.size.x), dispatch(viewport.size.y), 1);

    // swap the textures and return the one that contains this frame's result
    std::swap(resultBuffer, historyBuffer);
    return historyBuffer;
}

void TAAResolve::createRenderTargets(const Viewport& viewport) {
    glCreateTextures(GL_TEXTURE_2D, 1, &resultBuffer);
    glTextureStorage2D(resultBuffer, 1, GL_RGBA16F, viewport.size.x, viewport.size.y);
    glTextureParameteri(resultBuffer, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(resultBuffer, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(resultBuffer, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(resultBuffer, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(resultBuffer, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glCreateTextures(GL_TEXTURE_2D, 1, &historyBuffer);
    glTextureStorage2D(historyBuffer, 1, GL_RGBA16F, viewport.size.x, viewport.size.y);
    glTextureParameteri(historyBuffer, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(historyBuffer, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(historyBuffer, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(historyBuffer, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(historyBuffer, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

void TAAResolve::destroyRenderTargets() {
    GLuint textures[] = { resultBuffer, historyBuffer };
    glDeleteTextures(2, textures);
}

} // namespace Raekor