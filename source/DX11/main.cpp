#include "Raekor/application.h"
#include "Raekor/OS.h"
#include "Raekor/gui.h"
#include "Raekor/timer.h"
#include "Raekor/input.h"
#include "Raekor/rtti.h"
#include "DXShader.h"
#include "DXRenderer.h"
#include "DXResourceBuffer.h"

inline void gThrowIfFailed(HRESULT inResult) {
    if (FAILED(inResult))
        __debugbreak();
}

namespace Raekor {

struct DXGeometry {
    ComPtr<ID3D11Buffer> mIndexBuffer = nullptr;
    ComPtr<ID3D11Buffer> mVertexBuffer = nullptr;
    ComPtr<ID3D11ShaderResourceView> mVertexSRV = nullptr;

    bool IsReady() const { return mIndexBuffer && mVertexBuffer && mVertexSRV; }
};

struct DXMaterial {
    ComPtr<ID3D11Texture2D> mAlbedoTexture = nullptr;
    ComPtr<ID3D11ShaderResourceView> mAlbedoSRV = nullptr;
    ComPtr<ID3D11Texture2D> mNormalTexture = nullptr;
    ComPtr<ID3D11ShaderResourceView> mNormalSRV = nullptr;
    ComPtr<ID3D11Texture2D> mMetalRoughTexture = nullptr;
    ComPtr<ID3D11ShaderResourceView> mMetalRoughSRV = nullptr;

    bool IsReady() const { return mAlbedoTexture && mAlbedoSRV && mNormalTexture && mNormalSRV && mMetalRoughTexture && mMetalRoughSRV; }
};

bool UploadMesh(const Mesh& inMesh, DXGeometry& inGeometry) {
    const auto vertices = inMesh.GetInterleavedVertices();
    const auto vertices_size = vertices.size() * sizeof(vertices[0]);
    const auto indices_size = inMesh.indices.size() * sizeof(inMesh.indices[0]);

    if (!vertices_size || !indices_size)
        return false;

    {
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.ByteWidth = uint32_t(vertices_size);
        buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
        buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        buffer_desc.StructureByteStride = inMesh.GetInterleavedStride();

        D3D11_SUBRESOURCE_DATA vb_data = {};
        vb_data.pSysMem = &(vertices[0]);

        D3D.device->CreateBuffer(&buffer_desc, &vb_data, inGeometry.mVertexBuffer.GetAddressOf());

        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srv_desc.Format = DXGI_FORMAT_UNKNOWN;
        srv_desc.Buffer.NumElements = buffer_desc.ByteWidth / buffer_desc.StructureByteStride;
        D3D.device->CreateShaderResourceView(inGeometry.mVertexBuffer.Get(), &srv_desc, inGeometry.mVertexSRV.GetAddressOf());
    }

    {
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.ByteWidth = uint32_t(indices_size);
        buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
        buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vb_data = {};
        vb_data.pSysMem = &(inMesh.indices[0]);

        D3D.device->CreateBuffer(&buffer_desc, &vb_data, inGeometry.mIndexBuffer.GetAddressOf());
    }

    return true;
}


ComPtr<ID3D11Texture2D> UploadTexture(const TextureAsset::Ptr& inTexture, DXGI_FORMAT informat) {
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

    for (int mip = 0; mip < desc.MipLevels; mip++) {
        const auto dimensions = glm::ivec2(std::max(header_ptr->dwWidth >> mip, 1ul), std::max(header_ptr->dwHeight >> mip, 1ul));

        auto& subresource = subresources[mip];
        subresource.pSysMem = data_ptr;
        subresource.SysMemPitch = std::max(1, ((dimensions.x + 3) / 4)) * 16;

        data_ptr += dimensions.x * dimensions.y;
    }

    ComPtr<ID3D11Texture2D> texture = nullptr;
    D3D.device->CreateTexture2D(&desc, subresources.data(), texture.GetAddressOf());

    const auto debug_name = inTexture->GetPath().string();
    texture->SetPrivateData(WKPDID_D3DDebugObjectName, debug_name.size(), debug_name.data());

    return texture;
}


bool UploadMaterial(Assets& inAssets, const Material& inMaterial, DXMaterial& ioMaterial) {
    if (const auto asset = inAssets.GetAsset<TextureAsset>(inMaterial.albedoFile)) {
        ioMaterial.mAlbedoTexture = UploadTexture(asset, DXGI_FORMAT_BC3_UNORM_SRGB);
        gThrowIfFailed(D3D.device->CreateShaderResourceView(ioMaterial.mAlbedoTexture.Get(), nullptr, ioMaterial.mAlbedoSRV.GetAddressOf()));
    }

    if (const auto asset = inAssets.GetAsset<TextureAsset>(inMaterial.normalFile)) {
        ioMaterial.mNormalTexture = UploadTexture(asset, DXGI_FORMAT_BC3_UNORM);
        gThrowIfFailed(D3D.device->CreateShaderResourceView(ioMaterial.mNormalTexture.Get(), nullptr, ioMaterial.mNormalSRV.GetAddressOf()));
    }

    if (const auto asset = inAssets.GetAsset<TextureAsset>(inMaterial.metalroughFile)) {
        ioMaterial.mMetalRoughTexture = UploadTexture(asset, DXGI_FORMAT_BC3_UNORM);
        gThrowIfFailed(D3D.device->CreateShaderResourceView(ioMaterial.mMetalRoughTexture.Get(), nullptr, ioMaterial.mMetalRoughSRV.GetAddressOf()));
    }

    return true;
}

struct GBufferConstants {
    glm::vec4 mLightPosition = glm::vec4(0.0f, 5.0f, 0.0f, 1.0f);
    glm::vec4 mCameraPosition;
    glm::mat4 mModelMatrix;
    glm::mat4 mViewMatrix;
    glm::mat4 mProjectionMatrix;
};

class DX11App : public Application {
public:
    DX11App() : Application(WindowFlag::RESIZE), m_Renderer(m_Viewport, m_Window) {
        while (!FileSystem::exists(m_Settings.mSceneFile))
            m_Settings.mSceneFile = FileSystem::relative(OS::sOpenFileDialog("Scene Files (*.scene)\0*.scene\0")).string();

        SDL_SetWindowTitle(m_Window, std::string(m_Settings.mSceneFile + " - Raekor Renderer").c_str());
        m_Scene.OpenFromFile(m_Assets, m_Settings.mSceneFile);

        Timer timer;
        for (const auto& [entity, material] : m_Scene.view<Material>().each())
            m_Scene.emplace<DXMaterial>(entity);

        for (const auto& [entity, mesh] : m_Scene.view<Mesh>().each())
            m_Scene.emplace<DXGeometry>(entity);

        Async::sQueueJob([this]() {
            /// MESH DATA ///
            for (const auto& [entity, mesh, geometry] : m_Scene.view<Mesh, DXGeometry>().each())
                UploadMesh(mesh, geometry);

            ///// TEXTURE DATA ///
            const auto black_texture_file = TextureAsset::sConvert("assets/system/black4x4.png");
            const auto white_texture_file = TextureAsset::sConvert("assets/system/white4x4.png");
            const auto normal_texture_file = TextureAsset::sConvert("assets/system/normal4x4.png");
            m_DefaultBlackTexture = UploadTexture(m_Assets.GetAsset<TextureAsset>(black_texture_file), DXGI_FORMAT_BC3_UNORM);
            D3D.device->CreateShaderResourceView(m_DefaultBlackTexture.Get(), nullptr, m_DefaultBlackTextureSRV.GetAddressOf());
            m_DefaultWhiteTexture = UploadTexture(m_Assets.GetAsset<TextureAsset>(white_texture_file), DXGI_FORMAT_BC3_UNORM);
            D3D.device->CreateShaderResourceView(m_DefaultWhiteTexture.Get(), nullptr, m_DefaultWhiteTextureSRV.GetAddressOf());
            m_DefaultNormalTexture = UploadTexture(m_Assets.GetAsset<TextureAsset>(normal_texture_file), DXGI_FORMAT_BC3_UNORM);
            D3D.device->CreateShaderResourceView(m_DefaultNormalTexture.Get(), nullptr, m_DefaultNormalTextureSRV.GetAddressOf());

            for (const auto& [entity, material, dx_material] : m_Scene.view<Material, DXMaterial>().each())
                UploadMaterial(m_Assets, material, dx_material);
        });

        std::cout << std::format("Scene Upload took {:3f} ms.\n", Timer::sToMilliseconds(timer.GetElapsedTime()));

        const auto gbuffer_stages = std::array {
            DXShader::Stage { .type = DXShader::Type::VERTEX, .textfile = "assets/system/shaders/DirectX/old/simple_vertex.hlsl" },
            DXShader::Stage { .type = DXShader::Type::FRAG, .textfile = "assets/system/shaders/DirectX/old/simple_fp.hlsl" },
        };

        m_GbufferShader = DXShader(gbuffer_stages.data(), gbuffer_stages.size());
        m_GBufferConstantBuffer = DXResourceBuffer(sizeof(GBufferConstants));

        D3D11_SAMPLER_DESC sampler_desc = {};
        sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.MaxAnisotropy = 16.0f;
        sampler_desc.MaxLOD = FLT_MAX;

        D3D.device->CreateSamplerState(&sampler_desc, m_SamplerAnisoWrap.GetAddressOf());

        std::cout << "Render Size: " << m_Viewport.size.x << " , " << m_Viewport.size.y << '\n';

        m_Viewport.GetCamera().Move(Vec2(42.0f, 10.0f));
        m_Viewport.GetCamera().Zoom(5.0f);
        m_Viewport.GetCamera().Look(Vec2(1.65f, 0.2f));
        m_Viewport.SetFieldOfView(65.0f);
    }

    virtual void OnUpdate(float dt) override {
        // INPUT UPDATES
        m_Viewport.OnUpdate(dt);
        m_GBufferConstants.mViewMatrix = m_Viewport.GetCamera().GetView();
        m_GBufferConstants.mProjectionMatrix = m_Viewport.GetCamera().GetProjection();

        // IMGUI UPDATES
        m_Renderer.ImGui_NewFrame(m_Window);
        ImGuizmo::BeginFrame();

        ImGui::Begin("DX11 Settings");
        ImGui::Text("Dear ImGui %s", ImGui::GetVersion());
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::Checkbox("V-Sync", &m_Settings.mVsyncEnabled);
        ImGui::DragFloat3("Light", glm::value_ptr(m_GBufferConstants.mLightPosition), 0.1f, -200.0f, 200.0f);
        ImGui::End();

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

        if (manipulated) {
            light_transform.Decompose();
            m_GBufferConstants.mLightPosition = Vec4(light_transform.position, 1.0);
        }
        
        // DO THE RENDERING THINGS
        m_Renderer.Clear(Vec4(0, 0, 0, 1));
        m_Renderer.BindPipeline();
        m_GbufferShader.Bind();

        const auto samplers = std::array { m_SamplerAnisoWrap.Get() };
        D3D.context->PSSetSamplers(0, samplers.size(), samplers.data());

        for (const auto& [entity, transform, mesh, geometry] : m_Scene.view<Transform, Mesh, DXGeometry>().each()) {
            if (!geometry.IsReady())
                continue;

            auto material = m_Scene.try_get<DXMaterial>(mesh.material);
            if (material && !material->IsReady())
                continue;

            m_GBufferConstants.mModelMatrix = transform.worldTransform;

            m_GBufferConstantBuffer.Update(&m_GBufferConstants, sizeof(GBufferConstants));

            const auto cbs = std::array { m_GBufferConstantBuffer.GetBuffer() };
            D3D.context->VSSetConstantBuffers(0, cbs.size(), cbs.data());
            D3D.context->PSSetConstantBuffers(0, cbs.size(), cbs.data());

            const auto srvs = std::array { geometry.mVertexSRV.Get() };
            D3D.context->VSSetShaderResources(0, srvs.size(), srvs.data());


            if (material != nullptr) {
                auto textures = std::array { material->mAlbedoSRV.Get(), material->mNormalSRV.Get(), material->mMetalRoughSRV.Get() };
                D3D.context->PSSetShaderResources(0, textures.size(), textures.data());
            }
            else {
                auto textures = std::array { m_DefaultWhiteTextureSRV.Get(), m_DefaultNormalTextureSRV.Get(), m_DefaultWhiteTextureSRV.Get() };
                D3D.context->PSSetShaderResources(0, textures.size(), textures.data());
            }

            D3D.context->IASetIndexBuffer(geometry.mIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
            D3D.context->DrawIndexedInstanced(mesh.indices.size(), 1, 0, 0, 0);


        }

        m_Renderer.ImGui_Render();
        
        m_Renderer.SwapBuffers(m_Settings.mVsyncEnabled);

    }
    virtual void OnEvent(const SDL_Event& inEvent) override {
        ImGui_ImplSDL2_ProcessEvent(&inEvent);
        
        if (!ImGui::GetIO().WantCaptureMouse)
            CameraController::OnEvent(m_Viewport.GetCamera(), inEvent);

        if (inEvent.type == SDL_KEYDOWN && !inEvent.key.repeat) {
            switch (inEvent.key.keysym.sym) {
                case SDLK_r: {
                    m_GbufferShader.CheckForReload();
                } break;
            }
        }
    }

private:
    Scene m_Scene;
    Assets m_Assets;
    ComPtr<ID3D11Texture2D> m_DefaultBlackTexture;
    ComPtr<ID3D11ShaderResourceView> m_DefaultBlackTextureSRV;
    ComPtr<ID3D11Texture2D> m_DefaultWhiteTexture;
    ComPtr<ID3D11ShaderResourceView> m_DefaultWhiteTextureSRV;
    ComPtr<ID3D11Texture2D> m_DefaultNormalTexture;
    ComPtr<ID3D11ShaderResourceView> m_DefaultNormalTextureSRV;

    DXShader m_GbufferShader;
    ComPtr<ID3D11SamplerState> m_SamplerAnisoWrap;
    GBufferConstants m_GBufferConstants;
    DXResourceBuffer m_GBufferConstantBuffer;

    DXRenderer m_Renderer;
};

}

using namespace Raekor;

int main(int argc, char** argv) {
    RTTIFactory::Register(RTTI_OF(ConfigSettings));

    Raekor::Application* app = new Raekor::DX11App();

    app->Run();

    delete app;

    return 0;
}