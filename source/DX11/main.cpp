#include "Raekor/application.h"
#include "Raekor/OS.h"
#include "Raekor/gui.h"
#include "Raekor/timer.h"
#include "Raekor/input.h"
#include "Raekor/rtti.h"
#include "Raekor/physics.h"
#include "Raekor/launcher.h"
#include "Raekor/primitives.h"
#include "DXShader.h"
#include "DXRenderer.h"
#include "DXResourceBuffer.h"

namespace Raekor {

struct DXMesh
{
    RTTI_DECLARE_TYPE(DXMesh);

    ComPtr<ID3D11Buffer> mIndexBuffer = nullptr;
    ComPtr<ID3D11Buffer> mVertexBuffer = nullptr;
    ComPtr<ID3D11ShaderResourceView> mVertexSRV = nullptr;

    bool IsReady() const { return mIndexBuffer && mVertexBuffer && mVertexSRV; }
};
RTTI_DEFINE_TYPE(DXMesh) {}



struct DXMaterial
{
    RTTI_DECLARE_TYPE(DXMaterial);

    ComPtr<ID3D11Texture2D> mAlbedoTexture = nullptr;
    ComPtr<ID3D11ShaderResourceView> mAlbedoSRV = nullptr;
    ComPtr<ID3D11Texture2D> mNormalTexture = nullptr;
    ComPtr<ID3D11ShaderResourceView> mNormalSRV = nullptr;
    ComPtr<ID3D11Texture2D> mMetalRoughTexture = nullptr;
    ComPtr<ID3D11ShaderResourceView> mMetalRoughSRV = nullptr;

    bool IsReady() const { return mAlbedoTexture && mAlbedoSRV && mNormalTexture && mNormalSRV && mMetalRoughTexture && mMetalRoughSRV; }
};
RTTI_DEFINE_TYPE(DXMaterial) {}




struct GBufferConstants
{
    float mLODFade = 1.0f;
    float pad0;
    float metallic;
    float roughness;
    glm::vec4 mAlbedo;
    glm::vec4 mLightPosition = glm::vec4(0.0f, 5.0f, 0.0f, 1.0f);
    glm::vec4 mCameraPosition;
    glm::mat4 mModelMatrix;
    glm::mat4 mViewMatrix;
    glm::mat4 mProjectionMatrix;
};



class DX11App : public Application
{
public:
    bool UploadMesh(Mesh& inMesh, DXMesh& ioMesh)
    {
        const auto& vertices = inMesh.GetInterleavedVertices();
        const auto  vertices_size = vertices.size() * sizeof(vertices[0]);
        const auto  indices_size = inMesh.indices.size() * sizeof(inMesh.indices[0]);

        if (!vertices_size || !indices_size)
            return false;

        {
            D3D11_BUFFER_DESC buffer_desc = {};
            buffer_desc.ByteWidth = uint32_t(vertices_size);
            buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
            // "vertex" buffers are bound as structured buffers and the shader manually fetches the data by indexing into the buffer using SV_VertexID
            buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            buffer_desc.StructureByteStride = inMesh.GetInterleavedStride();

            D3D11_SUBRESOURCE_DATA vb_data = {};
            vb_data.pSysMem = &( vertices[0] );

            D3D.device->CreateBuffer(&buffer_desc, &vb_data, ioMesh.mVertexBuffer.GetAddressOf());

            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            srv_desc.Format = DXGI_FORMAT_UNKNOWN;
            srv_desc.Buffer.NumElements = buffer_desc.ByteWidth / buffer_desc.StructureByteStride;
            D3D.device->CreateShaderResourceView(ioMesh.mVertexBuffer.Get(), &srv_desc, ioMesh.mVertexSRV.GetAddressOf());
        }

        {
            D3D11_BUFFER_DESC buffer_desc = {};
            buffer_desc.ByteWidth = uint32_t(indices_size);
            buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
            buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

            D3D11_SUBRESOURCE_DATA vb_data = {};
            vb_data.pSysMem = &( inMesh.indices[0] );

            D3D.device->CreateBuffer(&buffer_desc, &vb_data, ioMesh.mIndexBuffer.GetAddressOf());
        }

        return true;
    }



    ComPtr<ID3D11Texture2D> UploadTexture(const TextureAsset::Ptr& inTexture, DXGI_FORMAT informat)
    {
        const auto header_ptr = inTexture->GetHeader();

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = header_ptr->dwWidth;
        desc.Height = header_ptr->dwHeight;
        desc.MipLevels = header_ptr->dwMipMapCount;
        desc.ArraySize = 1;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.Format = informat;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        auto data_ptr = inTexture->GetData();
        auto subresources = std::vector<D3D11_SUBRESOURCE_DATA>(desc.MipLevels);

        for (uint32_t mip = 0; mip < desc.MipLevels; mip++)
        {
            const auto dimensions = glm::ivec2(std::max(header_ptr->dwWidth >> mip, 1ul), std::max(header_ptr->dwHeight >> mip, 1ul));

            auto& subresource = subresources[mip];
            subresource.pSysMem = data_ptr;
            subresource.SysMemPitch = std::max(1, ( ( dimensions.x + 3 ) / 4 )) * 16;

            data_ptr += dimensions.x * dimensions.y;
        }

        ComPtr<ID3D11Texture2D> texture = nullptr;
        D3D.device->CreateTexture2D(&desc, subresources.data(), texture.GetAddressOf());

        const auto debug_name = inTexture->GetPath().string();
        texture->SetPrivateData(WKPDID_D3DDebugObjectName, uint32_t(debug_name.size()), debug_name.data());

        return texture;
    }

    bool UploadMaterial(Assets& inAssets, const Material& inMaterial, DXMaterial& ioMaterial)
    {
        if (const auto asset = inAssets.GetAsset<TextureAsset>(inMaterial.albedoFile))
        {
            ioMaterial.mAlbedoTexture = UploadTexture(asset, DXGI_FORMAT_BC3_UNORM_SRGB);
            gThrowIfFailed(D3D.device->CreateShaderResourceView(ioMaterial.mAlbedoTexture.Get(), nullptr, ioMaterial.mAlbedoSRV.GetAddressOf()));
        }
        else
        {
            ioMaterial.mAlbedoTexture = m_DefaultWhiteTexture;
            ioMaterial.mAlbedoSRV = m_DefaultWhiteTextureSRV;
        }

        if (const auto asset = inAssets.GetAsset<TextureAsset>(inMaterial.normalFile))
        {
            ioMaterial.mNormalTexture = UploadTexture(asset, DXGI_FORMAT_BC3_UNORM);
            gThrowIfFailed(D3D.device->CreateShaderResourceView(ioMaterial.mNormalTexture.Get(), nullptr, ioMaterial.mNormalSRV.GetAddressOf()));
        }
        else
        {
            ioMaterial.mNormalTexture = m_DefaultNormalTexture;
            ioMaterial.mNormalSRV = m_DefaultNormalTextureSRV;
        }

        if (const auto asset = inAssets.GetAsset<TextureAsset>(inMaterial.metallicFile))
        {
            ioMaterial.mMetalRoughTexture = UploadTexture(asset, DXGI_FORMAT_BC3_UNORM);
            gThrowIfFailed(D3D.device->CreateShaderResourceView(ioMaterial.mMetalRoughTexture.Get(), nullptr, ioMaterial.mMetalRoughSRV.GetAddressOf()));
        }
        else
        {
            ioMaterial.mMetalRoughTexture = m_DefaultWhiteTexture;
            ioMaterial.mMetalRoughSRV = m_DefaultWhiteTextureSRV;
        }

        return true;
    }

    DX11App() : Application(WindowFlag::RESIZE), m_Renderer(m_Viewport, m_Window), m_Scene(nullptr)
    {
        while (!fs::exists(m_Settings.mSceneFile))
            m_Settings.mSceneFile = fs::relative(OS::sOpenFileDialog("Scene Files (*.scene)\0*.scene\0")).string();

        SDL_SetWindowTitle(m_Window, std::string(m_Settings.mSceneFile.string() + " - Raekor Renderer").c_str());
        m_Scene.OpenFromFile(m_Assets, m_Settings.mSceneFile.string());

        Timer timer;
        for (const auto& [entity, material] : m_Scene.Each<Material>())
            m_Scene.Add<DXMaterial>(entity);

        for (const auto& [entity, mesh] : m_Scene.Each<Mesh>())
        {
            m_Scene.Add<DXMesh>(entity);
            m_StreamingJobsPending++;

            //if (m_CustomSettings.mEnableMeshFading)
                //mesh.mLODFade = 1.0f;
        }

        g_ThreadPool.QueueJob([this]()
        {
            const auto black_texture_file = TextureAsset::sConvert("assets/system/black4x4.png");
            const auto white_texture_file = TextureAsset::sConvert("assets/system/white4x4.png");
            const auto normal_texture_file = TextureAsset::sConvert("assets/system/normal4x4.png");
            m_DefaultBlackTexture = UploadTexture(m_Assets.GetAsset<TextureAsset>(black_texture_file), DXGI_FORMAT_BC3_UNORM);
            D3D.device->CreateShaderResourceView(m_DefaultBlackTexture.Get(), nullptr, m_DefaultBlackTextureSRV.GetAddressOf());
            m_DefaultWhiteTexture = UploadTexture(m_Assets.GetAsset<TextureAsset>(white_texture_file), DXGI_FORMAT_BC3_UNORM);
            D3D.device->CreateShaderResourceView(m_DefaultWhiteTexture.Get(), nullptr, m_DefaultWhiteTextureSRV.GetAddressOf());
            m_DefaultNormalTexture = UploadTexture(m_Assets.GetAsset<TextureAsset>(normal_texture_file), DXGI_FORMAT_BC3_UNORM);
            D3D.device->CreateShaderResourceView(m_DefaultNormalTexture.Get(), nullptr, m_DefaultNormalTextureSRV.GetAddressOf());

            for (const auto& [entity, mesh, geometry] : m_Scene.Each<Mesh, DXMesh>())
            {
                UploadMesh(mesh, geometry);

                if (m_Scene.IsValid(mesh.material))
                {
                    const auto& [material, dx_material] = m_Scene.Get<Material, DXMaterial>(mesh.material);
                    if (!dx_material.IsReady())
                        UploadMaterial(m_Assets, material, dx_material);
                }

                Sleep(100);
                m_StreamingJobsFinished.fetch_add(1);
            }
        });

        std::cout << std::format("Scene Upload took {:.2f} ms\n", Timer::sToMilliseconds(timer.GetElapsedTime()));

        const auto gbuffer_stages = std::array {
            DXShader::Stage {.type = DXShader::Type::VERTEX, .mTextFile = "assets/system/shaders/DirectX/old/simple_vertex.hlsl" },
                DXShader::Stage {.type = DXShader::Type::FRAG, .mTextFile = "assets/system/shaders/DirectX/old/simple_fp.hlsl" },
        };

        m_GbufferShader = DXShader(gbuffer_stages.data(), gbuffer_stages.size());

        const auto gbuffer_depth_stages = std::array {
            DXShader::Stage {.type = DXShader::Type::VERTEX, .mTextFile = "assets/system/shaders/DirectX/old/simple_vertex.hlsl" },
                DXShader::Stage {.type = DXShader::Type::FRAG, .mTextFile = "assets/system/shaders/DirectX/old/depth_fp.hlsl" },
        };

        m_GbufferDepthShader = DXShader(gbuffer_depth_stages.data(), gbuffer_depth_stages.size());

        m_GBufferConstantBuffer = DXResourceBuffer(sizeof(GBufferConstants));

        D3D11_SAMPLER_DESC sampler_desc = {};
        sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.MaxAnisotropy = 16u;
        sampler_desc.MaxLOD = FLT_MAX;

        D3D.device->CreateSamplerState(&sampler_desc, m_SamplerAnisoWrap.GetAddressOf());

        std::cout << "Render Size: " << m_Viewport.size.x << " , " << m_Viewport.size.y << '\n';

        m_Viewport.GetCamera().Move(Vec2(42.0f, 10.0f));
        m_Viewport.GetCamera().Zoom(5.0f);
        m_Viewport.GetCamera().Look(Vec2(1.65f, 0.2f));
        m_Viewport.SetFieldOfView(65.0f);
    }

    virtual void OnUpdate(float dt) override
    {
        // Check if any BoxCollider's are waiting to be registered
        m_Physics.OnUpdate(m_Scene);

        // Step the physics simulation
        if (m_Physics.GetState() == Physics::Stepping)
            m_Physics.Step(m_Scene, dt);

        // INPUT UPDATES
        m_Viewport.OnUpdate(dt);
        m_GBufferConstants.mViewMatrix = m_Viewport.GetCamera().GetView();
        m_GBufferConstants.mProjectionMatrix = m_Viewport.GetCamera().GetProjection();

        // update scene components
        m_Scene.UpdateTransforms();
        m_Scene.UpdateLights();

        // IMGUI UPDATES
        m_Renderer.ImGui_NewFrame(m_Window);
        ImGuizmo::BeginFrame();

        ImGui::Begin("DX11 Settings");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

        const auto meshes_streamed = m_StreamingJobsFinished.load();
        if (meshes_streamed != m_StreamingJobsPending)
        {
            const auto progress_string = std::format("({}/{}) Streaming Jobs..", meshes_streamed, m_StreamingJobsPending);
            ImGui::ProgressBar((float)meshes_streamed / (float)m_StreamingJobsPending, ImVec2(-FLT_MIN, 0), progress_string.c_str());
        }
        else
        {
            ImGui::ProgressBar(1.0f, ImVec2(-FLT_MIN, 0), "Streaming Finished.");
        }

        if (ImGui::SmallButton("Reset Mesh Fade"))
        {
            g_ThreadPool.QueueJob([this]()
            {
                for (const auto& [entity, mesh] : m_Scene.Each<Mesh>())
                {
                    mesh.mLODFade = 1.0f;
                    Sleep(rand() % 100);
                }
            });
        }

        if (ImGui::SmallButton("Generate Rigid Bodies"))
        {
            for (const auto& [sb_entity, sb_transform, sb_mesh] : m_Scene.Each<Transform, Mesh>())
            {
                m_Scene.Add<BoxCollider>(sb_entity);
                m_StreamingJobsPending++;
            }

            for (const auto& [entity, sb_transform, sb_mesh, sb_collider] : m_Scene.Each<Transform, Mesh, BoxCollider>())
            {
                auto& mesh = sb_mesh;
                auto& transform = sb_transform;
                auto& collider = sb_collider;

                g_ThreadPool.QueueJob([&]()
                {
                    JPH::TriangleList triangles;

                    for (int i = 0; i < mesh.indices.size(); i += 3)
                    {
                        auto v0 = mesh.positions[mesh.indices[i]];
                        auto v1 = mesh.positions[mesh.indices[i + 1]];
                        auto v2 = mesh.positions[mesh.indices[i + 2]];

                        v0 *= transform.scale;
                        v1 *= transform.scale;
                        v2 *= transform.scale;

                        triangles.push_back(JPH::Triangle(JPH::Float3(v0.x, v0.y, v0.z), JPH::Float3(v1.x, v1.y, v1.z), JPH::Float3(v2.x, v2.y, v2.z)));
                    }

                    collider.motionType = JPH::EMotionType::Static;
                    JPH::Ref<JPH::ShapeSettings> settings = new JPH::MeshShapeSettings(triangles);

                    auto body_settings = JPH::BodyCreationSettings(
                        settings,
                        JPH::Vec3(transform.position.x, transform.position.y, transform.position.z),
                        JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w),
                        collider.motionType,
                        EPhysicsObjectLayers::MOVING
                    );

                    auto& body_interface = m_Physics.GetSystem()->GetBodyInterface();
                    collider.bodyID = body_interface.CreateAndAddBody(body_settings, JPH::EActivation::Activate);

                    m_StreamingJobsFinished.fetch_add(1);
                });
            }
        }

        if (g_ThreadPool.GetActiveJobCount() == 0 && ImGui::Button("Spawn/Reset Balls"))
        {
            const auto material_entity = m_Scene.Create();

            auto& material_name = m_Scene.Add<Name>(material_entity);
            material_name = "Ball Material";

            auto& ball_material = m_Scene.Add<Material>(material_entity);
            ball_material.albedo = glm::vec4(1.0f, 0.25f, 0.38f, 1.0f);

            UploadMaterial(m_Assets, ball_material, m_Scene.Add<DXMaterial>(material_entity));

            for (uint32_t i = 0; i < 64; i++)
            {
                auto entity = m_Scene.CreateSpatialEntity("ball");
                auto& transform = m_Scene.Get<Transform>(entity);
                auto& mesh = m_Scene.Add<Mesh>(entity);

                mesh.material = material_entity;
                constexpr auto radius = 2.5f;
                gGenerateSphere(mesh, radius, 16, 16);
                UploadMesh(mesh, m_Scene.Add<DXMesh>(entity));

                transform.position = Vec3(-65.0f, 85.0f + i * ( radius * 2.0f ), 0.0f);
                transform.Compose();

                auto& collider = m_Scene.Add<BoxCollider>(entity);
                collider.motionType = JPH::EMotionType::Dynamic;
                JPH::Ref<JPH::ShapeSettings> settings = new JPH::SphereShapeSettings(radius);

                auto body_settings = JPH::BodyCreationSettings(
                    settings,
                    JPH::Vec3(transform.position.x, transform.position.y, transform.position.z),
                    JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w),
                    collider.motionType,
                    EPhysicsObjectLayers::MOVING
                );

                body_settings.mFriction = 0.1f;
                body_settings.mRestitution = 0.35f;

                auto& body_interface = m_Physics.GetSystem()->GetBodyInterface();
                collider.bodyID = body_interface.CreateAndAddBody(body_settings, JPH::EActivation::Activate);

                //body_interface.AddImpulse(collider.bodyID, JPH::Vec3Arg(50.0f, -2.0f, 50.0f));
            }
        }

        const auto physics_state = m_Physics.GetState();
        if (ImGui::SmallButton(physics_state == Physics::Stepping ? "pause" : "start"))
        {
            switch (physics_state)
            {
                case Physics::Idle:
                {
                    m_Physics.SaveState();
                    m_Physics.SetState(Physics::Stepping);
                } break;
                case Physics::Paused:
                {
                    m_Physics.SetState(Physics::Stepping);
                } break;
                case Physics::Stepping:
                {
                    m_Physics.SetState(Physics::Paused);
                } break;
            }
        }

        ImGui::SameLine();

        if (ImGui::SmallButton("stop"))
        {
            if (physics_state != Physics::Idle)
            {
                m_Physics.RestoreState();
                m_Physics.Step(m_Scene, dt); // Step once to trigger the restored state
                m_Physics.SetState(Physics::Idle);
            }
        }

        ImGui::Checkbox("V-Sync", &m_Settings.mVsyncEnabled);

        static bool show_gizmo = false;
        ImGui::Checkbox("Show Gizmo", &show_gizmo);

        ImGui::DragFloat3("Light Position", glm::value_ptr(m_GBufferConstants.mLightPosition), 0.1f, -200.0f, 200.0f);
        ImGui::End();

        if (show_gizmo)
        {
            ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
            ImGuizmo::SetRect(0, 0, float(m_Viewport.size.x), float(m_Viewport.size.y));

            Transform light_transform;
            light_transform.position = m_GBufferConstants.mLightPosition;
            light_transform.Compose();

            const auto manipulated = ImGuizmo::Manipulate(
                glm::value_ptr(m_Viewport.GetCamera().GetView()),
                glm::value_ptr(m_Viewport.GetCamera().GetProjection()),
                ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::MODE::WORLD,
                glm::value_ptr(light_transform.localTransform)
            );

            if (manipulated)
            {
                light_transform.Decompose();
                m_GBufferConstants.mLightPosition = Vec4(light_transform.position, 1.0);
            }
        }

        // DO THE RENDERING THINGS
        m_Renderer.Clear(Vec4(0, 0, 0, 1));
        m_Renderer.BindPipeline();

        // DEPTH PASS
        m_GbufferDepthShader.Bind(D3D.context.Get());
        D3D.context->OMSetRenderTargets(0, nullptr, D3D.depth_stencil_view.Get());

        for (const auto& [entity, transform, mesh, geometry] : m_Scene.Each<Transform, Mesh, DXMesh>())
        {
            if (!geometry.IsReady())
                continue;

            auto dx_material = m_Scene.GetPtr<DXMaterial>(mesh.material);
            if (dx_material && !dx_material->IsReady())
                continue;

            if (mesh.mLODFade == 0.0f)
                continue;


            m_GBufferConstants.mLODFade = mesh.mLODFade;
            m_GBufferConstants.mModelMatrix = transform.worldTransform;

            m_GBufferConstantBuffer.Update(&m_GBufferConstants, sizeof(GBufferConstants));

            const auto cbs = std::array { m_GBufferConstantBuffer.GetBuffer() };
            D3D.context->VSSetConstantBuffers(0, uint32_t(cbs.size()), cbs.data());
            D3D.context->PSSetConstantBuffers(0, uint32_t(cbs.size()), cbs.data());

            const auto srvs = std::array { geometry.mVertexSRV.Get() };
            D3D.context->VSSetShaderResources(0, uint32_t(srvs.size()), srvs.data());

            D3D.context->RSSetState(D3D.rasterize_state.Get());
            D3D.context->IASetIndexBuffer(geometry.mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
            D3D.context->DrawIndexedInstanced(uint32_t(mesh.indices.size()), 1, 0, 0, 0);

            // Fade in the mesh over a span of 4 seconds
            mesh.mLODFade = glm::max(0.0f, mesh.mLODFade - ( dt * 0.25f ));
        }

        // SCENE GEOMETRY PASS
        m_GbufferShader.Bind(D3D.context.Get());
        D3D.context->RSSetState(D3D.rasterize_state.Get());
        D3D.context->OMSetRenderTargets(1, D3D.back_buffer.GetAddressOf(), D3D.depth_stencil_view.Get());

        const auto samplers = std::array { m_SamplerAnisoWrap.Get() };
        D3D.context->PSSetSamplers(0, uint32_t(samplers.size()), samplers.data());

        for (const auto& [entity, transform, mesh, geometry] : m_Scene.Each<Transform, Mesh, DXMesh>())
        {
            if (!geometry.IsReady())
                continue;

            auto dx_material = m_Scene.GetPtr<DXMaterial>(mesh.material);
            if (dx_material && !dx_material->IsReady())
                continue;

            if (mesh.mLODFade == 0.0f)
                m_Renderer.BindPipeline();
            else
                m_Renderer.BindPipeline(D3D11_COMPARISON_EQUAL);

            auto& material = m_Scene.Get<Material>(mesh.material);

            m_GBufferConstants.mAlbedo = material.albedo;
            m_GBufferConstants.mLODFade = mesh.mLODFade;
            m_GBufferConstants.mModelMatrix = transform.worldTransform;

            m_GBufferConstantBuffer.Update(&m_GBufferConstants, sizeof(GBufferConstants));

            const auto cbs = std::array { m_GBufferConstantBuffer.GetBuffer() };
            D3D.context->VSSetConstantBuffers(0, uint32_t(cbs.size()), cbs.data());
            D3D.context->PSSetConstantBuffers(0, uint32_t(cbs.size()), cbs.data());

            const auto srvs = std::array { geometry.mVertexSRV.Get() };
            D3D.context->VSSetShaderResources(0, uint32_t(srvs.size()), srvs.data());


            if (dx_material != nullptr)
            {
                auto textures = std::array { dx_material->mAlbedoSRV.Get(), dx_material->mNormalSRV.Get(), dx_material->mMetalRoughSRV.Get() };
                D3D.context->PSSetShaderResources(0, uint32_t(textures.size()), textures.data());
            }
            else
            {
                auto textures = std::array { m_DefaultWhiteTextureSRV.Get(), m_DefaultNormalTextureSRV.Get(), m_DefaultWhiteTextureSRV.Get() };
                D3D.context->PSSetShaderResources(0, uint32_t(textures.size()), textures.data());
            }

            D3D.context->IASetIndexBuffer(geometry.mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
            D3D.context->DrawIndexedInstanced(uint32_t(mesh.indices.size()), 1, 0, 0, 0);
        }

        m_Renderer.ImGui_Render();

        m_Renderer.SwapBuffers(m_Settings.mVsyncEnabled);
    }


    virtual void OnEvent(const SDL_Event& inEvent) override
    {
        ImGui_ImplSDL2_ProcessEvent(&inEvent);

        if (!ImGui::GetIO().WantCaptureMouse)
            CameraController::OnEvent(m_Viewport.GetCamera(), inEvent);

        if (inEvent.type == SDL_KEYDOWN && !inEvent.key.repeat)
        {
            switch (inEvent.key.keysym.sym)
            {
                case SDLK_r:
                {
                    m_GbufferShader.CheckForReload();
                } break;
            }
        }

        if (inEvent.type == SDL_WINDOWEVENT && inEvent.window.event == SDL_WINDOWEVENT_CLOSE)
            g_ThreadPool.WaitForJobs(); // Wait for streaming to finish
    }

private:
    struct
    {
        int& mEnableMeshFading = g_CVars.Create("r_mesh_fading", 1);
    } m_CustomSettings;

    ComPtr<ID3D11SamplerState> m_SamplerAnisoWrap;
    ComPtr<ID3D11Texture2D> m_DefaultBlackTexture;
    ComPtr<ID3D11ShaderResourceView> m_DefaultBlackTextureSRV;
    ComPtr<ID3D11Texture2D> m_DefaultWhiteTexture;
    ComPtr<ID3D11ShaderResourceView> m_DefaultWhiteTextureSRV;
    ComPtr<ID3D11Texture2D> m_DefaultNormalTexture;
    ComPtr<ID3D11ShaderResourceView> m_DefaultNormalTextureSRV;

    Scene m_Scene;
    Assets m_Assets;
    Physics m_Physics;
    DXRenderer m_Renderer;

    DXShader m_GbufferShader;
    DXShader m_GbufferDepthShader;
    GBufferConstants m_GBufferConstants;
    DXResourceBuffer m_GBufferConstantBuffer;

    uint32_t m_StreamingJobsPending = 0;
    std::atomic<uint32_t> m_StreamingJobsFinished = 0;
};

}

using namespace Raekor;

int main(int argc, char** argv)
{
    g_RTTIFactory.Register(RTTI_OF(ConfigSettings));

    g_CVars.ParseCommandLine(argc, argv);

    auto should_launch = true;

    if (g_CVars.Create("enable_launcher", 0))
    {
        Launcher launcher;
        launcher.Run();

        should_launch = launcher.ShouldLaunch();
    }

    if (!should_launch)
        return 0;

    Raekor::Application* app = new Raekor::DX11App();

    app->Run();

    delete app;

    return 0;
}