#include "PCH.h"
#include "RenderPasses.h"
#include "Shader.h"
#include "RayTracing.h"

#include "Timer.h"
#include "Camera.h"
#include "Components.h"
#include "Primitives.h"
#include "UIRenderer.h"
#include "GPUProfiler.h"
#include "DebugRenderer.h"

namespace RK::DX12 {

/*

const T& AddPass(RenderGraph& inRenderGraph, Device& inDevice)
{
    return inRenderGraph.AddGraphicsPass<T>("pass_name",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, T& inData)
    {
    },
    [](T& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
    });
}

*/

bool RenderSettings::mDoFAutoFocus = true;
float RenderSettings::mDoFAperture = 1.8f;
float RenderSettings::mDoFFocusDistance = 5.0f;

int RenderSettings::mGTAOSliceCount = 2;
int RenderSettings::mGTAOStepCount = 8;
float RenderSettings::mGTAORadius = 1.0f;
float RenderSettings::mGTAOThickness = 0.25f;
float RenderSettings::mGTAOPower = 1.0f;

int RenderSettings::mSSRSamples = 16;
float RenderSettings::mSSRBias = 0.025f;
float RenderSettings::mSSRRadius = 0.05f;

float RenderSettings::mGrassBend = 0.0f;
float RenderSettings::mGrassTilt = 0.0f;
Vec2 RenderSettings::mWindDirection = Vec2(0.0f, -1.0f);

float RenderSettings::mEV100 = 15.0f;
float RenderSettings::mVignetteScale = 0.8f;
float RenderSettings::mVignetteBias = 0.2f;
float RenderSettings::mVignetteInner = 0.0f;
float RenderSettings::mVignetteOuter = 2.0f;
float RenderSettings::mBloomBlendFactor = 0.06f;
float RenderSettings::mChromaticAberrationStrength = 0.0;

float RenderSettings::mRTAORadius = 1.0;
float RenderSettings::mRTAOPower = 1.0;
float RenderSettings::mRTAONormalBias = 0.01;
uint32_t RenderSettings::mRTAOSampleCount = 1;

bool RenderSettings::mPathTraceReset = false;
uint32_t RenderSettings::mPathTraceBounces = 4u;

bool RenderSettings::mDDGIUseChebyshev = true;
bool RenderSettings::mDDGIUseMultibounce = true;
float RenderSettings::mDDGIDebugRadius = 0.25f;
IVec3 RenderSettings::mDDGIDebugProbe = IVec3(0, 0, 0);
IVec3 RenderSettings::mDDGIProbeCount = IVec3(16, 16, 16);
Vec3 RenderSettings::mDDGIProbeSpacing = Vec3(6.4, 3.0, 2.8);
Vec3 RenderSettings::mDDGICornerPosition = Vec3(-65, -1.4, -28.5);
bool RenderSettings::mDDGIFollowCamera = false;
uint32_t RenderSettings::mDDGICascadeCount = 1;
float RenderSettings::mDDGIIrradianceHysteresis = 0.97f;
bool RenderSettings::mDDGIRelocateAllProbes = false;
StaticArray<DDGIVolume, DDGI_MAX_CASCADES> RenderSettings::mDDGIVolumes = {};




static IVec3 sWrapProbeCoord(const IVec3& inCoord, const IVec3& inProbeCount)
{
    return ( ( inCoord % inProbeCount ) + inProbeCount ) % inProbeCount;
}



void RenderSettings::UpdateDDGIVolumes(const Vec3& inCameraPosition)
{
    const Vec3 static_center = mDDGICornerPosition + mDDGIProbeSpacing * Vec3(mDDGIProbeCount - 1) * 0.5f;

    for (uint32_t cascade = 0; cascade < mDDGIVolumes.size(); cascade++)
    {
        DDGIVolume& volume = mDDGIVolumes[cascade];
        volume = {};

        if (cascade >= mDDGICascadeCount)
            continue;

        volume.mProbeOffset = cascade * GetDDGIProbesPerCascade();
        volume.mProbeSpacing = mDDGIProbeSpacing * float(1u << cascade);

        if (mDDGIFollowCamera)
        {
            volume.mOriginCell = IVec3(glm::floor(inCameraPosition / volume.mProbeSpacing)) - mDDGIProbeCount / 2;
            volume.mScrollOffset = sWrapProbeCoord(volume.mOriginCell, mDDGIProbeCount);
            volume.mCornerPosition = Vec3(volume.mOriginCell) * volume.mProbeSpacing;
        }
        else
        {
            volume.mCornerPosition = static_center - volume.mProbeSpacing * Vec3(mDDGIProbeCount - 1) * 0.5f;
        }
    }
}



uint32_t RenderSettings::GetDDGIDebugProbeIndex()
{
    const IVec3 debug_probe = glm::clamp(mDDGIDebugProbe, IVec3(0), mDDGIProbeCount - 1);
    const IVec3 storage_coord = sWrapProbeCoord(debug_probe + mDDGIVolumes[0].mScrollOffset, mDDGIProbeCount);

    return storage_coord.x + storage_coord.y * mDDGIProbeCount.x + storage_coord.z * mDDGIProbeCount.x * mDDGIProbeCount.y;
}



DDGIData RenderSettings::GetDDGIData()
{
    return DDGIData
    {
        .mProbeCount = mDDGIProbeCount,
        .mProbeRadius = mDDGIDebugRadius,
        .mUseChebyshev = mDDGIUseChebyshev,
        .mCascadeCount = mDDGICascadeCount
    };
}



const DefaultTexturesData& AddDefaultTexturesPass(RenderGraph& inRenderGraph, Device& inDevice, TextureID inBlackTexture, TextureID inWhiteTexture)
{
    return inRenderGraph.AddGraphicsPass<DefaultTexturesData>("DefaultTextures",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, DefaultTexturesData& inData)
    {
        inData.mBlackTexture = ioRGBuilder.Import(inDevice, inBlackTexture);
        inData.mWhiteTexture = ioRGBuilder.Import(inDevice, inWhiteTexture);
    },
    [](DefaultTexturesData& inData, const RenderGraphResources& inResources, CommandList& inCmdList) {});
}



const ClearBufferData& AddClearBufferPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inBuffer, uint32_t inClearValue)
{
    return inRenderGraph.AddComputePass<ClearBufferData>(std::format("ClearBuffer"),
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ClearBufferData& inData)
    {
        inData.mBufferUAV = ioRGBuilder.Write(inBuffer);
    },
    [&inDevice, inClearValue](ClearBufferData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        inCmdList.PushComputeConstants(ClearBufferRootConstants
        {
            .mClearValue = inClearValue,
            .mBuffer = inResources.GetBindlessHeapIndex(inData.mBufferUAV)
        });

        const Buffer& buffer = inDevice.GetBuffer(inResources.GetBufferView(inData.mBufferUAV));

        inCmdList->SetPipelineState(g_SystemShaders.mClearBufferShader.GetComputePSO());
        inCmdList->Dispatch(( buffer.GetSize() + 63 ) / 64, 1, 1);
    });
}



const TransitionResourceData& AddTransitionResourcePass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphBuilderFunction inFunction, RenderGraphResourceID inResource)
{
    return inRenderGraph.AddGraphicsPass<TransitionResourceData>(std::format("EXPLICIT RESOURCE TRANSITION"),
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, TransitionResourceData& inData) { inData.mResourceView = (ioRGBuilder.*inFunction)(inResource); },
    [&inDevice](TransitionResourceData& inData, const RenderGraphResources& inResources, CommandList& inCmdList) {});
}



const SkyCubeData& AddSkyCubePass(RenderGraph& inRenderGraph, Device& inDevice, const RenderWorld& inWorld)
{
    return inRenderGraph.AddComputePass<SkyCubeData>("Skycube Generate",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, SkyCubeData& inData)
    {  
        if (inWorld.GetSun().mCubeMap.IsValid())
        {
            inData.mSkyCubeTexture = ioRGBuilder.Import(inDevice, inWorld.GetSun().mCubeMap);
            return;
        }

        inData.mSkyCubeTexture = ioRGBuilder.Create(Texture::Desc 
        {
            .format             = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .dimension          = Texture::TEX_DIM_CUBE,
            .width              = 64u,
            .height             = 64u, 
            .depthOrArrayLayers = 6u,
            .mipLevels          = 6u,
            .usage              = Texture::SHADER_READ_WRITE,
            .debugName          = "SkyCube"
        });

        ioRGBuilder.Write(inData.mSkyCubeTexture);
    },
    [&inDevice, &inWorld](SkyCubeData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        const RenderSun& sun = inWorld.GetSun();

        if (sun.mCubeMap.IsValid())
            return;

        inCmdList.PushComputeConstants(SkyCubeRootConstants
        {
            .mSkyCubeTexture    = inResources.GetBindlessHeapIndex(inData.mSkyCubeTexture),
            .mSunLightDirection = sun.mDirection,
            .mSunLightColor     = sun.mColor
        });

        inCmdList->SetPipelineState(g_SystemShaders.mSkyCubeShader.GetComputePSO());

        const Texture::Desc& texture_desc = inDevice.GetTexture(inResources.GetTexture(inData.mSkyCubeTexture)).GetDesc();

        inCmdList->Dispatch(texture_desc.width / 8, texture_desc.height / 8, texture_desc.depthOrArrayLayers);
    });
}



const ConvolveCubeData& AddConvolveSkyCubePass(RenderGraph& inRenderGraph, Device& inDevice, const RenderWorld& inWorld, const SkyCubeData& inSkyCubeData)
{
    return inRenderGraph.AddGraphicsPass<ConvolveCubeData>("Skycube Convolve",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ConvolveCubeData& inData)
    {
        uint32_t texture_width = 16u;
        uint32_t texture_height = 16u;

        if (inWorld.GetSun().mCubeMap.IsValid())
        {
            const Texture::Desc& desc = ioRGBuilder.GetResourceDesc(inSkyCubeData.mSkyCubeTexture).mTextureDesc;

            texture_width = desc.width;
            texture_height = desc.height;
        }

        inData.mConvolvedCubeTexture = ioRGBuilder.Create(Texture::DescCube
        (
            DXGI_FORMAT_R32G32B32A32_FLOAT, 
            texture_width, 
            texture_height, 
            Texture::SHADER_READ_WRITE
        ));

        ioRGBuilder.Write(inData.mConvolvedCubeTexture);
        inData.mCubeTextureSRV = ioRGBuilder.Read(inSkyCubeData.mSkyCubeTexture);
    },

    [&inDevice](ConvolveCubeData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        inCmdList->SetPipelineState(g_SystemShaders.mConvolveCubeShader.GetComputePSO());

        inCmdList.PushComputeConstants(ConvolveCubeRootConstants
        {
            .mCubeTexture = inResources.GetBindlessHeapIndex(inData.mCubeTextureSRV),
            .mConvolvedCubeTexture = inResources.GetBindlessHeapIndex(inData.mConvolvedCubeTexture)
        });

        const Texture& texture = inDevice.GetTexture(inResources.GetTexture(inData.mConvolvedCubeTexture));
        inCmdList->Dispatch(texture.GetWidth() / 8, texture.GetHeight() / 8, texture.GetLayers());
    });
}



const IntegrateBrdfData& AddIntegrateBrdfPass(RenderGraph& inRenderGraph, Device& inDevice)
{
    return inRenderGraph.AddGraphicsPass<IntegrateBrdfData>("Integrate Specular BRDF",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, IntegrateBrdfData& inData)
    {
        inData.outputTexture = ioRGBuilder.Create(Texture::Desc
        { 
            .format = DXGI_FORMAT_R16G16_FLOAT, 
            .width  = BRDF_LUT_SIZE, 
            .height = BRDF_LUT_SIZE, 
            .usage  = Texture::SHADER_READ_WRITE,
            .debugName = "BrdfLutTexture"
        });

        ioRGBuilder.Write(inData.outputTexture);
    },
    [&inDevice](IntegrateBrdfData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        if (inData.isGenerated)
            return;

        inCmdList.PushComputeConstants(IntegrateBrdfConstants
        {
            .mOutputTexture = inResources.GetBindlessHeapIndex(inData.outputTexture),
        });

        inCmdList->SetPipelineState(g_SystemShaders.mIntegrateBrdfShader.GetComputePSO());

        const Texture& texture= inDevice.GetTexture(inResources.GetTexture(inData.outputTexture));
        inCmdList->Dispatch(texture.GetWidth() / 8, texture.GetHeight() / 8, 1);

        inData.isGenerated = true;
    });
}



const SkinningData& AddSkinningPass(RenderGraph& inRenderGraph, Device& inDevice, const RenderWorld& inWorld)
{
    return inRenderGraph.AddComputePass<SkinningData>("Skinning",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, SkinningData& inData) 
    {
    },
    [&inDevice, &inWorld](SkinningData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        Slice<const RenderSkinnedMesh> skinned_meshes = inWorld.GetSkinnedMeshes();

        if (skinned_meshes.empty())
            return;

        inCmdList->SetPipelineState(g_SystemShaders.mSkinningShader.GetComputePSO());

        Slice<const Mat4x4> bone_matrices = inWorld.GetBoneMatrices();

        Array<D3D12_RESOURCE_BARRIER> post_skinning_barriers;
        post_skinning_barriers.reserve(skinned_meshes.size());

        for (const RenderSkinnedMesh& skinned_mesh : skinned_meshes)
        {
            PIXScopedEvent(static_cast<ID3D12GraphicsCommandList*>(inCmdList), PIX_COLOR(0, 255, 0), inWorld.GetName(skinned_mesh.mNameOffset));

            Buffer& bone_matrix_buffer = inDevice.GetBuffer(skinned_mesh.mBoneTransformsBuffer);

            const Mat4x4* bone_matrices_data = bone_matrices.data() + skinned_mesh.mBoneMatrixOffset;
            const size_t bone_matrices_size = skinned_mesh.mBoneMatrixCount * sizeof(Mat4x4);

            inDevice.UploadBufferData(inCmdList, bone_matrix_buffer, 0, bone_matrices_data, bone_matrices_size);

            auto copy_to_uav_barrier = CD3DX12_RESOURCE_BARRIER::Transition(bone_matrix_buffer.GetD3D12Resource(), D3D12_RESOURCE_STATE_COPY_DEST, GetD3D12ResourceStates(Texture::SHADER_READ_ONLY));
            inCmdList->ResourceBarrier(1, &copy_to_uav_barrier);

            inCmdList.PushComputeConstants(SkinningRootConstants 
            {
                .mBoneIndicesBuffer     = inDevice.GetBindlessHeapIndex(skinned_mesh.mBoneIndexBuffer),
                .mBoneWeightsBuffer     = inDevice.GetBindlessHeapIndex(skinned_mesh.mBoneWeightBuffer),
                .mMeshVertexBuffer      = inDevice.GetBindlessHeapIndex(skinned_mesh.mMeshVertexBuffer),
                .mSkinnedVertexBuffer   = inDevice.GetBindlessHeapIndex(skinned_mesh.mSkinnedVertexBuffer),
                .mBoneTransformsBuffer  = inDevice.GetBindlessHeapIndex(skinned_mesh.mBoneTransformsBuffer),
                .mDispatchSize          = skinned_mesh.mVertexCount
            });

            inCmdList->Dispatch((skinned_mesh.mVertexCount + 63) / 64, 1, 1);

            ID3D12Resource* skinned_vertex_buffer = inDevice.GetD3D12Resource(skinned_mesh.mSkinnedVertexBuffer);
            post_skinning_barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(skinned_vertex_buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE));
        }

        inCmdList->ResourceBarrier(uint32_t(post_skinning_barriers.size()), post_skinning_barriers.data());
    });
}



const GBufferData& AddGBufferPass(RenderGraph& inRenderGraph, Device& inDevice, const RenderWorld& inWorld)
{
    return inRenderGraph.AddGraphicsPass<GBufferData>("GBuffer",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, GBufferData& inData)
    {
        Texture::Desc render_texture_desc =
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = "RT_GBufferCompressed"
        };

        inData.mOutput.mRenderTexture = ioRGBuilder.Create(render_texture_desc);

        Texture::Desc selection_texture_desc = 
        {
            .format = DXGI_FORMAT_R32_UINT,
            .width  = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = "RT_GBufferSelection"
        };

        inData.mOutput.mSelectionTexture = ioRGBuilder.Create(selection_texture_desc);

        Texture::Desc velocity_texture_desc
        {
            .format = DXGI_FORMAT_R32G32_FLOAT,
            .width  = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = "RT_GBufferVelocity"
        };

        inData.mOutput.mVelocityTexture = ioRGBuilder.Create(velocity_texture_desc);

        Texture::Desc depth_stencil_texture_desc = 
        {
            .format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
            .width  = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage  = Texture::DEPTH_STENCIL_TARGET,
            .debugName = "RT_GBufferDepth"
        };

        inData.mOutput.mDepthTexture = ioRGBuilder.Create(depth_stencil_texture_desc);

        ioRGBuilder.RenderTarget(inData.mOutput.mRenderTexture); // SV_Target0
        ioRGBuilder.RenderTarget(inData.mOutput.mVelocityTexture); // SV_Target1
        ioRGBuilder.RenderTarget(inData.mOutput.mSelectionTexture); // SV_Target2
        ioRGBuilder.DepthStencilTarget(inData.mOutput.mDepthTexture);

        {   // GBuffer PSO
            D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inRenderPass->CreatePipelineStateDesc(inDevice, g_SystemShaders.mGBufferShader);

            inData.mOpaquePipeline = inDevice.CreateGraphicsPipeline(pso_state);
            inData.mOpaquePipeline->SetName(L"PSO_OPAQUE_GBUFFER");
        }

        { // GBuffer with Alpha Clip PSO
            D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inRenderPass->CreatePipelineStateDesc(inDevice, g_SystemShaders.mGBufferAlphaClipShader);
            pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

            inData.mMaskedPipeline = inDevice.CreateGraphicsPipeline(pso_state);
            inData.mMaskedPipeline->SetName(L"PSO_MASKED_GBUFFER");
        }

        // store the render pass ptr so we can access it during execution
        inData.mRenderPass = inRenderPass;
    },

    [&inDevice, &inWorld](GBufferData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        TextureID depth_texture     = inResources.GetTexture(inData.mOutput.mDepthTexture);
        TextureID render_texture    = inResources.GetTexture(inData.mOutput.mRenderTexture);
        TextureID velocity_texture  = inResources.GetTexture(inData.mOutput.mVelocityTexture);
        TextureID selection_texture = inResources.GetTexture(inData.mOutput.mSelectionTexture);

        constexpr Vec4 clear_color = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        inCmdList.ClearRenderTarget(inDevice, render_texture, clear_color);
        inCmdList.ClearRenderTarget(inDevice, velocity_texture, clear_color);
        inCmdList.ClearRenderTarget(inDevice, selection_texture, clear_color);

        constexpr float clear_depth_value = 1.0f;
        constexpr uint8_t clear_stencil_value = 0u;
        inCmdList.ClearDepthStencilTarget(inDevice, depth_texture, &clear_depth_value, &clear_stencil_value);

        inCmdList.SetViewportAndScissor(inDevice.GetTexture(render_texture));

        Slice<const RenderInstance> instances = inWorld.GetInstances();
        Slice<const RenderBatch> batches = inWorld.GetBatches();

        auto DrawInstances = [&](ERenderBlendMode inBlendMode, ID3D12PipelineState* inDefaultPipeline)
        {
            ID3D12PipelineState* bound_pipeline = inDefaultPipeline;
            inCmdList->SetPipelineState(bound_pipeline);

            for (const RenderBatch& batch : batches)
            {
                const RenderInstance& instance = instances[batch.mFirstInstance];

                if (instance.mBlendMode != inBlendMode)
                    continue;

                ID3D12PipelineState* pipeline_state = inDefaultPipeline;

                if (instance.mVertexShader && instance.mPixelShader)
                {
                    pipeline_state = g_ShaderCompiler.GetGraphicsPipeline(inDevice, inData.mRenderPass, instance.mVertexShader, instance.mPixelShader);

                    if (pipeline_state == nullptr)
                        continue;
                }

                if (pipeline_state != bound_pipeline)
                {
                    inCmdList->SetPipelineState(pipeline_state);
                    bound_pipeline = pipeline_state;
                }

                EVENT_SCOPE_GPU(inCmdList, instance.mNameOffset ? inWorld.GetName(instance.mNameOffset) : "Mesh");

                inCmdList.PushGraphicsConstants(GbufferRootConstants
                {
                    .mEntity = uint32_t(instance.mEntity),
                    .mInstanceIndex = batch.mFirstInstance,
                });

                inCmdList.BindIndexBuffer(inDevice.GetBuffer(instance.mIndexBuffer));
                inCmdList.DrawIndexed(instance.mIndexCount, batch.mInstanceCount, 0, 0, 0);
            }
        };

        DrawInstances(RENDER_BLEND_MODE_OPAQUE, inData.mOpaquePipeline.Get());
        DrawInstances(RENDER_BLEND_MODE_MASKED, inData.mMaskedPipeline.Get());
    });
}



const GBufferDebugData& AddGBufferDebugPass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferOutput& inGBuffer, EDebugTexture inDebugTexture)
{
    return inRenderGraph.AddGraphicsPass<GBufferDebugData>("GBufferDebug",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, GBufferDebugData& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = "RT_GBufferDebug"
        });

        ioRGBuilder.RenderTarget(inData.mOutputTexture);

        switch (inDebugTexture)
        {
            case DEBUG_TEXTURE_GBUFFER_DEPTH:     inData.mInputTextureSRV = ioRGBuilder.Read(inGBuffer.mDepthTexture);    break;
            case DEBUG_TEXTURE_GBUFFER_ALBEDO:    inData.mInputTextureSRV = ioRGBuilder.Read(inGBuffer.mRenderTexture);   break;
            case DEBUG_TEXTURE_GBUFFER_NORMALS:   inData.mInputTextureSRV = ioRGBuilder.Read(inGBuffer.mRenderTexture);   break;
            case DEBUG_TEXTURE_GBUFFER_EMISSIVE:  inData.mInputTextureSRV = ioRGBuilder.Read(inGBuffer.mRenderTexture);   break;
            case DEBUG_TEXTURE_GBUFFER_VELOCITY:  inData.mInputTextureSRV = ioRGBuilder.Read(inGBuffer.mVelocityTexture); break;
            case DEBUG_TEXTURE_GBUFFER_METALLIC:  inData.mInputTextureSRV = ioRGBuilder.Read(inGBuffer.mRenderTexture);   break;
            case DEBUG_TEXTURE_GBUFFER_ROUGHNESS: inData.mInputTextureSRV = ioRGBuilder.Read(inGBuffer.mRenderTexture);   break;
            default: assert(false);
        }


        const GraphicsProgram* graphics_program = nullptr;

        switch (inDebugTexture)
        {
            case DEBUG_TEXTURE_GBUFFER_DEPTH:     graphics_program = &g_SystemShaders.mGBufferDebugDepthShader;     break;
            case DEBUG_TEXTURE_GBUFFER_ALBEDO:    graphics_program = &g_SystemShaders.mGBufferDebugAlbedoShader;    break;
            case DEBUG_TEXTURE_GBUFFER_NORMALS:   graphics_program = &g_SystemShaders.mGBufferDebugNormalsShader;   break;
            case DEBUG_TEXTURE_GBUFFER_EMISSIVE:  graphics_program = &g_SystemShaders.mGBufferDebugEmissiveShader;  break;
            case DEBUG_TEXTURE_GBUFFER_VELOCITY:  graphics_program = &g_SystemShaders.mGBufferDebugVelocityShader;  break;
            case DEBUG_TEXTURE_GBUFFER_METALLIC:  graphics_program = &g_SystemShaders.mGBufferDebugMetallicShader;  break;
            case DEBUG_TEXTURE_GBUFFER_ROUGHNESS: graphics_program = &g_SystemShaders.mGBufferDebugRoughnessShader; break;
            default: assert(false);
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = inRenderPass->CreatePipelineStateDesc(inDevice, *graphics_program);
        pso_desc.InputLayout = {}; // clear the input layout, we generate the fullscreen triangle inside the vertex shader
        pso_desc.DepthStencilState.DepthEnable = FALSE;
        pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inData.mPipeline = inDevice.CreateGraphicsPipeline(pso_desc);
        inData.mPipeline->SetName(L"PSO_GBUFFER_DEBUG");
    },

    [&inRenderGraph, &inDevice](GBufferDebugData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.SetViewportAndScissor(inRenderGraph.GetViewport());
        inCmdList.PushGraphicsConstants(GbufferDebugRootConstants
        {
            .mTexture   = inRGResources.GetBindlessHeapIndex(inData.mInputTextureSRV),
            .mFarPlane  = inRenderGraph.GetViewport().GetFar(),
            .mNearPlane = inRenderGraph.GetViewport().GetNear(),
        });
        inCmdList->DrawInstanced(3, 1, 0, 0);
    });
}



const TransparentForwardData& AddTransparentForwardPass(RenderGraph& inRenderGraph, Device& inDevice, const RenderWorld& inWorld, const GBufferOutput& inGBuffer, RenderGraphResourceID inRenderTarget,
    RenderGraphResourceID inBrdfLutTexture, RenderGraphResourceID inSkyCubeTexture, RenderGraphResourceID inDiffuseSkyCubeTexture, const DDGIOutput* inDDGI, bool inUseRayTracedShadows)
{
    return inRenderGraph.AddGraphicsPass<TransparentForwardData>("Transparent Forward",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, TransparentForwardData& inData)
    {
        inData.mOutputTextureRTV    = ioRGBuilder.RenderTarget(inRenderTarget);
        inData.mSelectionTextureRTV = ioRGBuilder.RenderTarget(inGBuffer.mSelectionTexture);
        inData.mDepthTextureDSV     = ioRGBuilder.DepthStencilTarget(inGBuffer.mDepthTexture);

        inData.mBrdfLutTextureSRV        = ioRGBuilder.Read(inBrdfLutTexture);
        inData.mSkyCubeTextureSRV        = ioRGBuilder.Read(inSkyCubeTexture);
        inData.mDiffuseSkyCubeTextureSRV = ioRGBuilder.Read(inDiffuseSkyCubeTexture);

        if (inDDGI)
        {
            inData.mVolumesBufferSRV           = ioRGBuilder.Read(inDDGI->mVolumes);
            inData.mProbeDataBufferSRV         = ioRGBuilder.Read(inDDGI->mProbeData);
            inData.mProbesDepthTextureSRV      = ioRGBuilder.Read(inDDGI->mDepthProbes);
            inData.mProbesIrradianceTextureSRV = ioRGBuilder.Read(inDDGI->mIrradianceProbes);
        }

        inData.mUseIndirectDiffuse = inDDGI != nullptr;
        inData.mUseRayTracedShadows = inUseRayTracedShadows;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inRenderPass->CreatePipelineStateDesc(inDevice, g_SystemShaders.mTransparentForwardShader);
        pso_state.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

        pso_state.BlendState.IndependentBlendEnable = TRUE;
        pso_state.BlendState.RenderTarget[0].BlendEnable = TRUE;
        pso_state.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
        pso_state.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        pso_state.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        pso_state.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        pso_state.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        pso_state.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        pso_state.BlendState.RenderTarget[1].BlendEnable = FALSE;

        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
        inData.mBackFacePipeline = inDevice.CreateGraphicsPipeline(pso_state);
        inData.mBackFacePipeline->SetName(L"PSO_TRANSPARENT_FORWARD_BACK_FACES");

        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        inData.mFrontFacePipeline = inDevice.CreateGraphicsPipeline(pso_state);
        inData.mFrontFacePipeline->SetName(L"PSO_TRANSPARENT_FORWARD_FRONT_FACES");
    },

    [&inRenderGraph, &inDevice, &inWorld](TransparentForwardData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        struct SortedDraw
        {
            uint32_t mInstanceIndex;
            float mDistance;
        };

        Array<SortedDraw> draws;

        const Vec3 camera_position = inRenderGraph.GetViewport().GetPosition();

        Slice<const RenderInstance> instances = inWorld.GetInstances();

        for (uint32_t instance_index = 0; instance_index < instances.size(); instance_index++)
        {
            const RenderInstance& instance = instances[instance_index];

            if (instance.mBlendMode != RENDER_BLEND_MODE_TRANSPARENT || instance.HasCustomShaders())
                continue;

            const Vec3 to_camera = instance.mBoundsCenter - camera_position;
            draws.push_back(SortedDraw { instance_index, glm::dot(to_camera, to_camera) });
        }

        if (draws.empty())
            return;

        std::sort(draws.begin(), draws.end(), [](const SortedDraw& inLeft, const SortedDraw& inRight) { return inLeft.mDistance > inRight.mDistance; });

        TransparentForwardConstants root_constants =
        {
            .mBrdfLutTexture        = inResources.GetBindlessHeapIndex(inData.mBrdfLutTextureSRV),
            .mSkyCubeTexture        = inResources.GetBindlessHeapIndex(inData.mSkyCubeTextureSRV),
            .mDiffuseSkyCubeTexture = inResources.GetBindlessHeapIndex(inData.mDiffuseSkyCubeTextureSRV),
            .mUseRayTracedShadows   = inData.mUseRayTracedShadows,
            .mUseIndirectDiffuse    = inData.mUseIndirectDiffuse,
        };

        if (inData.mUseIndirectDiffuse)
        {
            root_constants.mDDGIData = RenderSettings::GetDDGIData();
            root_constants.mDDGIData.mVolumesBuffer = inResources.GetBindlessHeapIndex(inData.mVolumesBufferSRV);
            root_constants.mDDGIData.mProbesDataBuffer = inResources.GetBindlessHeapIndex(inData.mProbeDataBufferSRV);
            root_constants.mDDGIData.mProbesDepthTexture = inResources.GetBindlessHeapIndex(inData.mProbesDepthTextureSRV);
            root_constants.mDDGIData.mProbesIrradianceTexture = inResources.GetBindlessHeapIndex(inData.mProbesIrradianceTextureSRV);
        }

        inCmdList.SetViewportAndScissor(inDevice.GetTexture(inResources.GetTextureView(inData.mOutputTextureRTV)));

        for (const SortedDraw& draw : draws)
        {
            const RenderInstance& instance = instances[draw.mInstanceIndex];

            EVENT_SCOPE_GPU(inCmdList, instance.mNameOffset ? inWorld.GetName(instance.mNameOffset) : "Mesh");

            root_constants.mEntity = uint32_t(instance.mEntity);
            root_constants.mInstanceIndex = draw.mInstanceIndex;

            inCmdList.PushGraphicsConstants(root_constants);
            inCmdList.BindIndexBuffer(inDevice.GetBuffer(instance.mIndexBuffer));

            inCmdList->SetPipelineState(inData.mBackFacePipeline.Get());
            inCmdList.DrawIndexed(instance.mIndexCount, 1, 0, 0, 0);

            inCmdList->SetPipelineState(inData.mFrontFacePipeline.Get());
            inCmdList.DrawIndexed(instance.mIndexCount, 1, 0, 0, 0);
        }
    });
}



const GTAOData& AddGTAOPass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferOutput& inGBuffer)
{
    return inRenderGraph.AddComputePass<GTAOData>("GTAO",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, GTAOData& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16_FLOAT,
            .width  = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage  = Texture::Usage::SHADER_READ_WRITE,
            .debugName = "GTAO"
        });

        ioRGBuilder.Write(inData.mOutputTexture);

        inData.mDepthTexture   = ioRGBuilder.Read(inGBuffer.mDepthTexture);
        inData.mGBufferTexture = ioRGBuilder.Read(inGBuffer.mRenderTexture);
    },

    [&inRenderGraph, &inDevice](GTAOData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        const Viewport& viewport = inRenderGraph.GetViewport();

        inCmdList.PushComputeConstants(GTAORootConstants
        {
            .mOutputTexture  = inRGResources.GetBindlessHeapIndex(inData.mOutputTexture),
            .mDepthTexture   = inRGResources.GetBindlessHeapIndex(inData.mDepthTexture),
            .mGBufferTexture = inRGResources.GetBindlessHeapIndex(inData.mGBufferTexture),
            .mSliceCount     = uint32_t(RenderSettings::mGTAOSliceCount),
            .mStepCount      = uint32_t(RenderSettings::mGTAOStepCount),
            .mRadius         = RenderSettings::mGTAORadius,
            .mThickness      = RenderSettings::mGTAOThickness,
            .mPower          = RenderSettings::mGTAOPower,
            .mDispatchSize   = viewport.GetRenderSize()
        });

        inCmdList->SetPipelineState(g_SystemShaders.mGTAOShader.GetComputePSO());
        inCmdList.Dispatch(( viewport.GetRenderSize().x + 7 ) / 8, ( viewport.GetRenderSize().y + 7 ) / 8, 1);
    });
}



const SSRTraceData& AddSSRTracePass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferOutput& inGBuffer, RenderGraphResourceID inSceneTexture)
{
    return inRenderGraph.AddComputePass<SSRTraceData>("SSR Trace",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, SSRTraceData& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
            {
                .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
                .width  = inRenderGraph.GetViewport().GetRenderSize().x,
                .height = inRenderGraph.GetViewport().GetRenderSize().y,
                .usage  = Texture::Usage::SHADER_READ_WRITE
            });

        inData.mSceneTexture = ioRGBuilder.Read(inSceneTexture);
        inData.mDepthTexture = ioRGBuilder.Read(inGBuffer.mDepthTexture);
        inData.mGBufferTexture = ioRGBuilder.Read(inGBuffer.mRenderTexture);
    },

    [&inRenderGraph, &inDevice](SSRTraceData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        const Viewport& viewport = inRenderGraph.GetViewport();

        inCmdList.PushComputeConstants(SSRTraceRootConstants
        {
            .mOutputTexture  = inRGResources.GetBindlessHeapIndex(inData.mOutputTexture),
            .mSceneTexture   = inRGResources.GetBindlessHeapIndex(inData.mSceneTexture),
            .mDepthTexture   = inRGResources.GetBindlessHeapIndex(inData.mDepthTexture),
            .mGBufferTexture = inRGResources.GetBindlessHeapIndex(inData.mGBufferTexture),
            .mRadius         = RenderSettings::mSSRRadius,
            .mBias           = RenderSettings::mSSRBias,
            .mSamples        = uint32_t(RenderSettings::mSSRSamples),
            .mDispatchSize   = viewport.GetRenderSize()
        });

        inCmdList->SetPipelineState(g_SystemShaders.mSSRTraceShader.GetComputePSO());
        inCmdList->Dispatch(( viewport.GetRenderSize().x + 7 ) / 8, ( viewport.GetRenderSize().y + 7 ) / 8, 1);
    });
}



const GrassData& AddGrassRenderPass(RenderGraph& inGraph, Device& inDevice, const GBufferOutput& inGBuffer)
{
    return inGraph.AddGraphicsPass<GrassData>("Grass",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, GrassData& inData)
    {
        inData.mRenderTextureSRV = ioRGBuilder.RenderTarget(inGBuffer.mRenderTexture);
        inData.mDepthTextureSRV  = ioRGBuilder.DepthStencilTarget(inGBuffer.mDepthTexture);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = inRenderPass->CreatePipelineStateDesc(inDevice, g_SystemShaders.mGrassShader);
        pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inData.mPipeline = inDevice.CreateGraphicsPipeline(pso_desc);
        inData.mPipeline->SetName(L"PSO_GRASS_DRAW");
    },

    [](GrassData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        const int blade_vertex_count = 15;

        inCmdList.PushGraphicsConstants(GrassRenderRootConstants 
        {
            .mBend = RenderSettings::mGrassBend,
            .mTilt = RenderSettings::mGrassTilt,
            .mWindDirection = RenderSettings::mWindDirection
        });

        inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList->DrawInstanced(blade_vertex_count, 65536, 0, 0);
        inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    });
}



const DownsampleData& AddDownsamplePass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inSourceTexture, const char* inPassName)
{
    auto CalculateMipCount = [](const uint32_t inWidth, const uint32_t inHeight) -> uint32_t
    {
        uint32_t max_res = glm::max(inWidth, inHeight);
        return uint32_t((glm::min(glm::floor(glm::log2(float(max_res))), 12.0f)));
    };

    return inRenderGraph.AddComputePass<DownsampleData>(inPassName,
    [&](RenderGraphBuilder& inRGBuilder, IRenderPass* inRenderPass, DownsampleData& inData)
    {
        inData.mGlobalAtomicBuffer = inRGBuilder.Create(Buffer::RWStructuredBuffer(sizeof(uint32_t), sizeof(uint32_t), "AtomicUintBuffer"));

        inData.mSourceTextureUAV = inRGBuilder.Write(inSourceTexture);
        /* inData.mGlobalAtomicBuffer */ inRGBuilder.Write(inData.mGlobalAtomicBuffer);

        const RenderGraphResourceDesc& texture_desc = inRGBuilder.GetResourceDesc(inSourceTexture);
        assert(texture_desc.mResourceType == RESOURCE_TYPE_TEXTURE);

        // const uint32_t nr_of_mips = CalculateMipCount(texture_desc.mTextureDesc.width, texture_desc.mTextureDesc.height);

        const uint32_t nr_of_mips = inRGBuilder.GetResourceDesc(inSourceTexture).mTextureDesc.mipLevels;
        // RK_ASSERT(nr_of_mips > 1);

        for (int mip = 0u; mip < nr_of_mips; mip++)
            inData.mSourceTextureMipsUAVs[mip] = inRGBuilder.WriteTexture(inSourceTexture, mip);
    },

    [&inRenderGraph, &inDevice, CalculateMipCount](DownsampleData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        Texture& texture = inDevice.GetTexture(inResources.GetTextureView(inData.mSourceTextureUAV));
        const UVec4 rect_info = UVec4(0u, 0u, texture->GetDesc().Width, texture->GetDesc().Height);

        // nothing to downsample if the texture only has 1 mip
        if (texture.GetMipCount() == 1)
            return;

        UVec2 work_group_offset, dispatchThreadGroupCountXY, numWorkGroupsAndMips;
        work_group_offset[0] = rect_info[0] / 64; // rectInfo[0] = left
        work_group_offset[1] = rect_info[1] / 64; // rectInfo[1] = top

        uint32_t endIndexX = ( rect_info[0] + rect_info[2] - 1 ) / 64; // rectInfo[0] = left, rectInfo[2] = width
        uint32_t endIndexY = ( rect_info[1] + rect_info[3] - 1 ) / 64; // rectInfo[1] = top, rectInfo[3] = height

        dispatchThreadGroupCountXY[0] = endIndexX + 1 - work_group_offset[0];
        dispatchThreadGroupCountXY[1] = endIndexY + 1 - work_group_offset[1];

        numWorkGroupsAndMips[0] = ( dispatchThreadGroupCountXY[0] ) * ( dispatchThreadGroupCountXY[1] );
        numWorkGroupsAndMips[1] = CalculateMipCount(rect_info[2], rect_info[3]);

        Buffer& atomic_buffer = inDevice.GetBuffer(inResources.GetBuffer(inData.mGlobalAtomicBuffer));

        SpdRootConstants root_constants = SpdRootConstants
        {
            .mNrOfMips = numWorkGroupsAndMips[1],
            .mNrOfWorkGroups = numWorkGroupsAndMips[0],
            .mGlobalAtomicBuffer = inResources.GetBindlessHeapIndex(inData.mGlobalAtomicBuffer),
            .mWorkGroupOffset = work_group_offset,
        };

        uint* mips_ptr = &root_constants.mTextureMip0;

        for (uint32_t mip = 0u; mip < numWorkGroupsAndMips[1]; mip++)
            mips_ptr[mip] = inResources.GetBindlessHeapIndex(inData.mSourceTextureMipsUAVs[mip]);

        inCmdList.PushComputeConstants(root_constants);

        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(atomic_buffer.GetD3D12Resource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
        inCmdList->ResourceBarrier(1, &barrier);

        const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER param = { .Dest = atomic_buffer->GetGPUVirtualAddress(), .Value = 0 };
        inCmdList->WriteBufferImmediate(1, &param, nullptr);

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(atomic_buffer.GetD3D12Resource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        inCmdList->ResourceBarrier(1, &barrier);

        inCmdList->SetPipelineState(g_SystemShaders.mDownsampleShader.GetComputePSO());
        inCmdList->Dispatch(dispatchThreadGroupCountXY.x, dispatchThreadGroupCountXY.y, texture.GetLayers());
    });
}



const TiledLightCullingData& AddTiledLightCullingPass(RenderGraph& inRenderGraph, Device& inDevice)
{
    return inRenderGraph.AddComputePass<TiledLightCullingData>("LightCull",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, TiledLightCullingData& inData)
    {
        const UVec2 render_size = inRenderGraph.GetViewport().GetRenderSize();
        const UVec2 nr_of_tiles = UVec2
        (
            (render_size.x + LIGHT_CULL_TILE_SIZE - 1) / LIGHT_CULL_TILE_SIZE, 
            (render_size.y + LIGHT_CULL_TILE_SIZE - 1) / LIGHT_CULL_TILE_SIZE
        );

        inData.mLightGridBuffer = ioRGBuilder.Create(Buffer::RWByteAddressBuffer(nr_of_tiles.x * nr_of_tiles.y, "LightCullingLightGrid"));
        inData.mLightIndicesBuffer = ioRGBuilder.Create(Buffer::RWByteAddressBuffer(nr_of_tiles.x * nr_of_tiles.y * LIGHT_CULL_MAX_LIGHTS, "LightCullingIndices"));

        ioRGBuilder.Write(inData.mLightGridBuffer);
        ioRGBuilder.Write(inData.mLightIndicesBuffer);
    },
    [&inRenderGraph](TiledLightCullingData& inData, const RenderGraphResources& inRGResources, CommandList& inCmdList)
    {
        inData.mRootConstants = 
        {
            .mLightGridBuffer    = inRGResources.GetBindlessHeapIndex(inData.mLightGridBuffer),
            .mLightIndicesBuffer = inRGResources.GetBindlessHeapIndex(inData.mLightIndicesBuffer),
        };

        inData.mRootConstants.mFullResSize = inRenderGraph.GetViewport().GetRenderSize();
        inData.mRootConstants.mDispatchSize.x = (inData.mRootConstants.mFullResSize.x + LIGHT_CULL_TILE_SIZE - 1) / LIGHT_CULL_TILE_SIZE;
        inData.mRootConstants.mDispatchSize.y = (inData.mRootConstants.mFullResSize.y + LIGHT_CULL_TILE_SIZE - 1) / LIGHT_CULL_TILE_SIZE;

        //inCmdList.PushComputeConstants(inData.mRootConstants);
        //inCmdList->SetPipelineState(g_SystemShaders.mLightCullShader.GetComputePSO());
//
        //inCmdList->Dispatch(inData.mRootConstants.mDispatchSize.x, inData.mRootConstants.mDispatchSize.y, 1);
    });
}



const LightingData& AddLightingPass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferOutput& inGBuffer, const TiledLightCullingData& inLightCullData, RenderGraphResourceID inBrdfLutTexture, RenderGraphResourceID inSkyCubeTexture, RenderGraphResourceID inDiffuseCubeTexture, RenderGraphResourceID inShadowTexture, RenderGraphResourceID inReflectionsTexture, RenderGraphResourceID inAOTexture, RenderGraphResourceID inIndirectDiffuseTexture, bool inUseReflectionsTexture, bool inUseIndirectDiffuseTexture)
{
    return inRenderGraph.AddGraphicsPass<LightingData>("Shading",

    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, LightingData& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width  = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = "RT_ShadingOutput"
        });

        ioRGBuilder.RenderTarget(inData.mOutputTexture);

        ioRGBuilder.Write(inLightCullData.mLightGridBuffer);
        ioRGBuilder.Write(inLightCullData.mLightIndicesBuffer);

        inData.mBrdfLutTextureSRV           = ioRGBuilder.Read(inBrdfLutTexture);
        inData.mSkyCubeTextureSRV           = ioRGBuilder.Read(inSkyCubeTexture);
        inData.mDiffuseSkyCubeTextureSRV    = ioRGBuilder.Read(inDiffuseCubeTexture);
        inData.mAmbientOcclusionTextureSRV  = ioRGBuilder.Read(inAOTexture);
        inData.mShadowMaskTextureSRV        = ioRGBuilder.Read(inShadowTexture);
        inData.mReflectionsTextureSRV       = ioRGBuilder.Read(inReflectionsTexture);
        inData.mIndirectDiffuseTextureSRV   = ioRGBuilder.Read(inIndirectDiffuseTexture);

        inData.mGBufferDepthTextureSRV      = ioRGBuilder.Read(inGBuffer.mDepthTexture);
        inData.mGBufferRenderTextureSRV     = ioRGBuilder.Read(inGBuffer.mRenderTexture);

        inData.mUseReflectionsTexture       = inUseReflectionsTexture;
        inData.mUseIndirectDiffuseTexture   = inUseIndirectDiffuseTexture;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inRenderPass->CreatePipelineStateDesc(inDevice, g_SystemShaders.mLightingShader);

        pso_state.InputLayout = {}; // clear the input layout, we generate the fullscreen triangle inside the vertex shader
        pso_state.DepthStencilState.DepthEnable = FALSE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inData.mPipeline = inDevice.CreateGraphicsPipeline(pso_state);
        inData.mPipeline->SetName(L"PSO_DEFERRED_LIGHTING");
    },

    [&inRenderGraph, &inDevice, &inLightCullData](LightingData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        //constexpr Vec4 clear_color = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        //inCmdList->ClearRenderTargetView(inDevice.GetCPUDescriptorHandle(inResources.GetTexture(inData.mOutputTexture)), glm::value_ptr(clear_color), 0, nullptr);

        LightingRootConstants root_constants =
        {
            .mBrdfLutTexture          = inResources.GetBindlessHeapIndex(inData.mBrdfLutTextureSRV),
            .mUseReflectionsTexture   = inData.mUseReflectionsTexture,
            .mUseIndirectDiffuseTexture = inData.mUseIndirectDiffuseTexture,
            .mSkyCubeTexture          = inResources.GetBindlessHeapIndex(inData.mSkyCubeTextureSRV),
            .mDiffuseSkyCubeTexture   = inResources.GetBindlessHeapIndex(inData.mDiffuseSkyCubeTextureSRV),
            .mShadowMaskTexture       = inResources.GetBindlessHeapIndex(inData.mShadowMaskTextureSRV),
            .mReflectionsTexture      = inResources.GetBindlessHeapIndex(inData.mReflectionsTextureSRV),
            .mGbufferDepthTexture     = inResources.GetBindlessHeapIndex(inData.mGBufferDepthTextureSRV),
            .mGbufferRenderTexture    = inResources.GetBindlessHeapIndex(inData.mGBufferRenderTextureSRV),
            .mIndirectDiffuseTexture  = inResources.GetBindlessHeapIndex(inData.mIndirectDiffuseTextureSRV),
            .mAmbientOcclusionTexture = inResources.GetBindlessHeapIndex(inData.mAmbientOcclusionTextureSRV),
        };

        root_constants.mLights = inLightCullData.mRootConstants;

        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.SetViewportAndScissor(inRenderGraph.GetViewport());
        inCmdList.PushGraphicsConstants(root_constants);
        inCmdList->DrawInstanced(3, 1, 0, 0);
    });
}



const TAAResolveData& AddTAAResolvePass(RenderGraph& inRenderGraph, Device& inDevice, const GBufferOutput& inGBuffer, RenderGraphResourceID inColorTexture)
{
    const TAAResolveData& taa_resolve_data = inRenderGraph.AddGraphicsPass<TAAResolveData>("TAA Resolve",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, TAAResolveData& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage = Texture::RENDER_TARGET,
            .debugName = "RT_TAAOutput"
        });

        inData.mHistoryTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width = inRenderGraph.GetViewport().GetRenderSize().x,
            .height = inRenderGraph.GetViewport().GetRenderSize().y,
            .usage = Texture::SHADER_READ_ONLY,
            .debugName = "RT_TAAHistory"
        });

        ioRGBuilder.RenderTarget(inData.mOutputTexture);

        inData.mColorTextureSRV = ioRGBuilder.Read(inColorTexture);
        inData.mHistoryTextureSRV = ioRGBuilder.Read(inData.mHistoryTexture);
        inData.mDepthTextureSRV = ioRGBuilder.Read(inGBuffer.mDepthTexture);
        inData.mVelocityTextureSRV = ioRGBuilder.Read(inGBuffer.mVelocityTexture);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inRenderPass->CreatePipelineStateDesc(inDevice, g_SystemShaders.mTAAResolveShader);

        pso_state.InputLayout = {}; // clear the input layout, we generate the fullscreen triangle inside the vertex shader
        pso_state.DepthStencilState.DepthEnable = FALSE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inData.mPipeline = inDevice.CreateGraphicsPipeline(pso_state);
        inData.mPipeline->SetName(L"PSO_TAA_RESOLVE");
    },

    [&inRenderGraph, &inDevice](TAAResolveData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {

        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.SetViewportAndScissor(inRenderGraph.GetViewport());

        inCmdList.PushGraphicsConstants(TAAResolveConstants 
        {
            .mRenderSize      = inRenderGraph.GetViewport().GetRenderSize(),
            .mRenderSizeRcp   = 1.0f / Vec2(inRenderGraph.GetViewport().GetRenderSize()),
            .mColorTexture    = inResources.GetBindlessHeapIndex(inData.mColorTextureSRV),
            .mDepthTexture    = inResources.GetBindlessHeapIndex(inData.mDepthTextureSRV),
            .mHistoryTexture  = inResources.GetBindlessHeapIndex(inData.mHistoryTextureSRV),
            .mVelocityTexture = inResources.GetBindlessHeapIndex(inData.mVelocityTextureSRV)
        });

        inCmdList->DrawInstanced(3, 1, 0, 0);

#if 1
        ID3D12Resource* result_texture_resource = inDevice.GetD3D12Resource(inResources.GetTexture(inData.mOutputTexture));
        ID3D12Resource* history_texture_resource = inDevice.GetD3D12Resource(inResources.GetTexture(inData.mHistoryTexture));

        std::array barriers =
        {
            D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(result_texture_resource, GetD3D12ResourceStates(Texture::RENDER_TARGET), D3D12_RESOURCE_STATE_COPY_SOURCE)),
            D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(history_texture_resource, GetD3D12ResourceStates(Texture::SHADER_READ_ONLY), D3D12_RESOURCE_STATE_COPY_DEST))
        };
        inCmdList->ResourceBarrier(barriers.size(), barriers.data());

        const CD3DX12_TEXTURE_COPY_LOCATION dest = CD3DX12_TEXTURE_COPY_LOCATION(history_texture_resource, 0);
        const CD3DX12_TEXTURE_COPY_LOCATION source = CD3DX12_TEXTURE_COPY_LOCATION(result_texture_resource, 0);
        inCmdList->CopyTextureRegion(&dest, 0, 0, 0, &source, nullptr);

        for (D3D12_RESOURCE_BARRIER& barrier : barriers)
            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

        inCmdList->ResourceBarrier(barriers.size(), barriers.data());
#endif
    });

#if 0
    inRenderGraph.AddGraphicsPass<CopyTextureGraphicsData>("TAA Copy History",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, CopyTextureGraphicsData& inData)
    {
        inData.mSrcTextureSRV = ioRGBuilder.Read(taa_resolve_data.mOutputTexture);
        inData.mDstTextureRTV = ioRGBuilder.RenderTarget(taa_resolve_data.mHistoryTexture);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inRenderPass->CreatePipelineStateDesc(inDevice, g_SystemShaders.mCopyTextureShader);

        pso_state.InputLayout = {}; // clear the input layout, we generate the fullscreen triangle inside the vertex shader
        pso_state.DepthStencilState.DepthEnable = FALSE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inData.mPipeline = inDevice.CreateGraphicsPipeline(pso_state);
        inData.mPipeline->SetName(L"PSO_COPY_TEXTURE");
    },

    [&inRenderGraph, &inDevice](CopyTextureGraphicsData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {

        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.SetViewportAndScissor(inDevice.GetTexture(inResources.GetTextureView(inData.mDstTextureRTV)));

        inCmdList.PushGraphicsConstants(CopyTextureConstants
        {
            .mSrcTexture = inResources.GetBindlessHeapIndex(inData.mSrcTextureSRV),
        });

        inCmdList->DrawInstanced(3, 1, 0, 0);
    });
#endif

    return taa_resolve_data;
}


const DepthOfFieldData& AddDepthOfFieldPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inInputTexture, RenderGraphResourceID inDepthTexture)
{
    struct DoFSetupData
    {
        UVec2 mFullSize;
        UVec2 mHalfSize;
        UVec2 mTileCount;
        RenderGraphResourceID mHalfResTexture;
        RenderGraphResourceViewID mInputTextureSRV;
        RenderGraphResourceViewID mDepthTextureSRV;
    };

    struct DoFTileMaxData
    {
        RenderGraphResourceID mTileTexture;
        RenderGraphResourceViewID mHalfResTextureSRV;
    };

    struct DoFGatherData
    {
        RenderGraphResourceID mBlurTexture;
        RenderGraphResourceViewID mTileTextureSRV;
        RenderGraphResourceViewID mHalfResTextureSRV;
    };

    auto GetRootConstants = [&inRenderGraph](UVec2 inDispatchSize, uint32_t inFullHeight)
    {
        constexpr float sensor_height = 0.024f;

        const Viewport& viewport = inRenderGraph.GetViewport();
        const float focal_length = 0.5f * sensor_height * viewport.GetProjection()[1][1];
        const float aperture = glm::max(RenderSettings::mDoFAperture, 0.1f);

        return DepthOfFieldRootConstants
        {
            .mAutoFocus = RenderSettings::mDoFAutoFocus,
            .mFocusDistance = glm::max(RenderSettings::mDoFFocusDistance, viewport.GetNear()),
            .mFocalLength = focal_length,
            .mLensCoefficient = ( focal_length * focal_length / aperture ) * ( float(inFullHeight) / sensor_height ),
            .mMaxCoC = 48.0f,
            .mNearPlane = viewport.GetNear(),
            .mFarPlane = viewport.GetFar(),
            .mDispatchSize = inDispatchSize
        };
    };

    const DoFSetupData& setup_data = inRenderGraph.AddComputePass<DoFSetupData>("DoF Setup",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, DoFSetupData& inData)
    {
        const Texture::Desc& input_desc = ioRGBuilder.GetResourceDesc(inInputTexture).mTextureDesc;

        inData.mFullSize = UVec2(input_desc.width, input_desc.height);
        inData.mHalfSize = ( inData.mFullSize + 1u ) / 2u;
        inData.mTileCount = ( inData.mHalfSize + UVec2(DOF_TILE_SIZE - 1) ) / UVec2(DOF_TILE_SIZE);

        inData.mHalfResTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = inData.mHalfSize.x,
            .height = inData.mHalfSize.y,
            .usage  = Texture::SHADER_READ_WRITE,
            .debugName = "RT_DoFHalfRes"
        });

        ioRGBuilder.Write(inData.mHalfResTexture);

        inData.mInputTextureSRV = ioRGBuilder.Read(inInputTexture);
        inData.mDepthTextureSRV = ioRGBuilder.Read(inDepthTexture);
    },
    [GetRootConstants](DoFSetupData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        DepthOfFieldRootConstants root_constants = GetRootConstants(inData.mHalfSize, inData.mFullSize.y);
        root_constants.mInputTexture = inResources.GetBindlessHeapIndex(inData.mInputTextureSRV);
        root_constants.mDepthTexture = inResources.GetBindlessHeapIndex(inData.mDepthTextureSRV);
        root_constants.mHalfResTexture = inResources.GetBindlessHeapIndex(inData.mHalfResTexture);

        inCmdList.PushComputeConstants(root_constants);
        inCmdList->SetPipelineState(g_SystemShaders.mDoFSetupShader.GetComputePSO());
        inCmdList->Dispatch(( inData.mHalfSize.x + 7 ) / 8, ( inData.mHalfSize.y + 7 ) / 8, 1);
    });

    const DoFTileMaxData& tile_max_data = inRenderGraph.AddComputePass<DoFTileMaxData>("DoF Tile Max",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, DoFTileMaxData& inData)
    {
        inData.mTileTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16_FLOAT,
            .width  = setup_data.mTileCount.x,
            .height = setup_data.mTileCount.y,
            .usage  = Texture::SHADER_READ_WRITE,
            .debugName = "RT_DoFTileMax"
        });

        ioRGBuilder.Write(inData.mTileTexture);

        inData.mHalfResTextureSRV = ioRGBuilder.Read(setup_data.mHalfResTexture);
    },
    [GetRootConstants, &setup_data](DoFTileMaxData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        DepthOfFieldRootConstants root_constants = GetRootConstants(setup_data.mTileCount, setup_data.mFullSize.y);
        root_constants.mHalfResTexture = inResources.GetBindlessHeapIndex(inData.mHalfResTextureSRV);
        root_constants.mTileTexture = inResources.GetBindlessHeapIndex(inData.mTileTexture);

        inCmdList.PushComputeConstants(root_constants);
        inCmdList->SetPipelineState(g_SystemShaders.mDoFTileMaxShader.GetComputePSO());
        inCmdList->Dispatch(setup_data.mTileCount.x, setup_data.mTileCount.y, 1);
    });

    const DoFGatherData& gather_data = inRenderGraph.AddComputePass<DoFGatherData>("DoF Gather",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, DoFGatherData& inData)
    {
        inData.mBlurTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = setup_data.mHalfSize.x,
            .height = setup_data.mHalfSize.y,
            .usage  = Texture::SHADER_READ_WRITE,
            .debugName = "RT_DoFBlur"
        });

        ioRGBuilder.Write(inData.mBlurTexture);

        inData.mTileTextureSRV = ioRGBuilder.Read(tile_max_data.mTileTexture);
        inData.mHalfResTextureSRV = ioRGBuilder.Read(setup_data.mHalfResTexture);
    },
    [GetRootConstants, &setup_data](DoFGatherData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        DepthOfFieldRootConstants root_constants = GetRootConstants(setup_data.mHalfSize, setup_data.mFullSize.y);
        root_constants.mHalfResTexture = inResources.GetBindlessHeapIndex(inData.mHalfResTextureSRV);
        root_constants.mTileTexture = inResources.GetBindlessHeapIndex(inData.mTileTextureSRV);
        root_constants.mBlurTexture = inResources.GetBindlessHeapIndex(inData.mBlurTexture);

        inCmdList.PushComputeConstants(root_constants);
        inCmdList->SetPipelineState(g_SystemShaders.mDoFGatherShader.GetComputePSO());
        inCmdList->Dispatch(( setup_data.mHalfSize.x + 7 ) / 8, ( setup_data.mHalfSize.y + 7 ) / 8, 1);
    });

    return inRenderGraph.AddComputePass<DepthOfFieldData>("DoF Composite",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, DepthOfFieldData& inData)
    {
        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .width  = setup_data.mFullSize.x,
            .height = setup_data.mFullSize.y,
            .usage  = Texture::SHADER_READ_WRITE,
            .debugName = "RT_DoFOutput"
        });

        ioRGBuilder.Write(inData.mOutputTexture);

        inData.mInputTextureSRV = ioRGBuilder.Read(inInputTexture);
        inData.mDepthTextureSRV = ioRGBuilder.Read(inDepthTexture);
        inData.mBlurTextureSRV = ioRGBuilder.Read(gather_data.mBlurTexture);
    },
    [GetRootConstants, &setup_data](DepthOfFieldData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        DepthOfFieldRootConstants root_constants = GetRootConstants(setup_data.mFullSize, setup_data.mFullSize.y);
        root_constants.mInputTexture = inResources.GetBindlessHeapIndex(inData.mInputTextureSRV);
        root_constants.mDepthTexture = inResources.GetBindlessHeapIndex(inData.mDepthTextureSRV);
        root_constants.mBlurTexture = inResources.GetBindlessHeapIndex(inData.mBlurTextureSRV);
        root_constants.mOutputTexture = inResources.GetBindlessHeapIndex(inData.mOutputTexture);

        inCmdList.PushComputeConstants(root_constants);
        inCmdList->SetPipelineState(g_SystemShaders.mDoFCompositeShader.GetComputePSO());
        inCmdList->Dispatch(( setup_data.mFullSize.x + 7 ) / 8, ( setup_data.mFullSize.y + 7 ) / 8, 1);
    });
}



const LuminanceHistogramData& AddLuminanceHistogramPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inInputTexture) 
{
    return inRenderGraph.AddComputePass<LuminanceHistogramData>("LuminanceHistogram",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, LuminanceHistogramData& inData)
    {
        inData.mHistogramBuffer = ioRGBuilder.Create(Buffer::RWTypedBuffer(DXGI_FORMAT_R32_UINT, 128, "LumHistogramBuffer"));
        inData.mInputTextureSRV = ioRGBuilder.Write(inInputTexture);
    },

    [](LuminanceHistogramData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
    });
}



const BloomPassData& AddBloomPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inInputTexture)
{
    auto RunBloomPass = [&](const String& inPassName, ID3D12PipelineState* inPipeline, RenderGraphResourceID inFromTexture, RenderGraphResourceID inToTexture, uint32_t inFromMip, uint32_t inToMip)
    {
        return inRenderGraph.AddComputePass<BloomBlurData>(inPassName,
        [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, BloomBlurData& inData)
        {
            inData.mToTextureMip   = inToMip;
            inData.mFromTextureMip = inFromMip;
            inData.mToTextureUAV   = ioRGBuilder.WriteTexture(inToTexture, inToMip);
            inData.mFromTextureSRV = ioRGBuilder.ReadTexture(inFromTexture, inFromMip);
        },
        [&inRenderGraph, &inDevice, inPipeline](BloomBlurData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
        {
            const CD3DX12_VIEWPORT to_viewport = CD3DX12_VIEWPORT(inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mToTextureUAV)), inData.mToTextureMip);
            const CD3DX12_VIEWPORT from_viewport = CD3DX12_VIEWPORT(inDevice.GetD3D12Resource(inResources.GetTextureView(inData.mFromTextureSRV)), inData.mFromTextureMip);
            
            inCmdList.PushComputeConstants(BloomRootConstants
            {
                .mSrcTexture   = inResources.GetBindlessHeapIndex(inData.mFromTextureSRV),
                .mSrcMipLevel  = inData.mFromTextureMip,
                .mDstTexture   = inResources.GetBindlessHeapIndex(inData.mToTextureUAV),
                .mDstMipLevel  = inData.mToTextureMip,
                .mDispatchSize = UVec2(to_viewport.Width, to_viewport.Height),
                .mSrcSizeRcp   = Vec2(1.0f / from_viewport.Width, 1.0f / from_viewport.Height)
            });

            inCmdList->SetPipelineState(inPipeline);
            inCmdList->Dispatch(( to_viewport.Width + 7 ) / 8, ( to_viewport.Height + 7 ) / 8, 1);
        });
    };
    

    auto AddDownsamplePass = [&](RenderGraphResourceID inFromTexture, RenderGraphResourceID inToTexture, uint32_t inFromMip, uint32_t inToMip)
    {
        return RunBloomPass(std::format("Bloom mip {} -> mip {}", inFromMip, inToMip), g_SystemShaders.mBloomDownsampleShader.GetComputePSO(), inFromTexture, inToTexture, inFromMip, inToMip);
    };

    auto AddUpsamplePass = [&](RenderGraphResourceID inFromTexture, RenderGraphResourceID inToTexture, uint32_t inFromMip, uint32_t inToMip)
    {
        return RunBloomPass(std::format("Bloom mip {} -> mip {}", inFromMip, inToMip), g_SystemShaders.mBloomUpSampleShader.GetComputePSO(), inFromTexture, inToTexture, inFromMip, inToMip);
    };


    return inRenderGraph.AddComputePass<BloomPassData>("Bloom",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, BloomPassData& inData)
    {
        const uint32_t cMipLevels = 6u;

        inData.mOutputTexture = ioRGBuilder.Create(Texture::Desc
        {
            .format    = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .width     = inRenderGraph.GetViewport().GetRenderSize().x,
            .height    = inRenderGraph.GetViewport().GetRenderSize().y,
            .mipLevels = cMipLevels,
            .usage     = Texture::SHADER_READ_WRITE,
            .debugName = "RT_BloomResult"
        });

        ioRGBuilder.Write(inData.mOutputTexture);

        // do an initial downsample from the current final scene texture to the first mip of the bloom chain
        AddDownsamplePass(inInputTexture, inData.mOutputTexture, 0, 1);

        // downsample along the bloom chain
        for (uint32_t mip = 1u; mip < cMipLevels - 1; mip++)
            AddDownsamplePass(inData.mOutputTexture, inData.mOutputTexture, mip, mip + 1);

        // upsample along the bloom chain
        for (uint32_t mip = cMipLevels - 1; mip > 0; mip--)
            AddUpsamplePass(inData.mOutputTexture, inData.mOutputTexture, mip, mip - 1);
    },

    [&inDevice](BloomPassData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        // clear or discard maybe?
    });
}



const DebugPrimitivesData& AddDebugOverlayPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inRenderTarget, RenderGraphResourceID inDepthTarget)
{
    constexpr size_t cPrimitiveBufferSize = gAlignUp(8 * 1024 * 1024, 4);

    return inRenderGraph.AddGraphicsPass<DebugPrimitivesData>("DebugShapes",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, DebugPrimitivesData& inData)
    {
        inData.mRenderTarget = ioRGBuilder.RenderTarget(inRenderTarget);
        inData.mDepthTarget = ioRGBuilder.DepthStencilTarget(inDepthTarget);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inRenderPass->CreatePipelineStateDesc(inDevice, g_SystemShaders.mDebugPrimitivesShader);
        pso_state.BlendState.RenderTarget[0].BlendEnable = true;
        pso_state.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        pso_state.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        pso_state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso_state.RasterizerState.AntialiasedLineEnable = true;
        //pso_state.InputLayout = {};

        inData.mPipeline = inDevice.CreateGraphicsPipeline(pso_state);
        inData.mPipeline->SetName(L"PSO_DEBUG_PRIM_LINES");

        inRenderPass->ReserveMemory(cPrimitiveBufferSize);
    },
    [&inRenderGraph](DebugPrimitivesData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        Slice<const Vec4> line_vertices = g_DebugRenderer.GetLinesToRender();

        if (line_vertices.empty())
            return;

        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.SetViewportAndScissor(inRenderGraph.GetViewport());
        
        const int size_in_bytes = glm::min(line_vertices.size_bytes(), cPrimitiveBufferSize);
        uint32_t vertex_data_offset = inRenderGraph.GetPerPassAllocator().AllocAndCopy(size_in_bytes, line_vertices.data());
        inCmdList.PushGraphicsConstants(DebugPrimitivesRootConstants { .mBufferOffset = vertex_data_offset });

        uint32_t capacity = inRenderGraph.GetPerPassAllocator().GetCapacity();
        uint32_t calculated_capacity = vertex_data_offset + sizeof(float4) * line_vertices.size();
        RK_ASSERT(capacity >= calculated_capacity);

        inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
        inCmdList->DrawInstanced(line_vertices.size(), 1, 0, 0);

        // restore triangle state for other passes
        inCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    });
}



static uint32_t sGetWireframeColor(Entity inEntity)
{
    uint32_t hash = uint32_t(inEntity) * 2654435761u;
    hash ^= hash >> 16;

    const float hue = float(hash & 0xFFFF) / 65535.0f * 6.0f;
    const float x = 1.0f - glm::abs(glm::mod(hue, 2.0f) - 1.0f);

    Vec3 rgb = Vec3(1.0f, x, 0.0f);

    switch (int(hue) % 6)
    {
        case 1: rgb = Vec3(x, 1.0f, 0.0f); break;
        case 2: rgb = Vec3(0.0f, 1.0f, x); break;
        case 3: rgb = Vec3(0.0f, x, 1.0f); break;
        case 4: rgb = Vec3(x, 0.0f, 1.0f); break;
        case 5: rgb = Vec3(1.0f, 0.0f, x); break;
    }

    rgb = glm::mix(Vec3(1.0f), rgb, 0.7f);

    const UVec3 bytes = UVec3(glm::round(rgb * 255.0f));
    return bytes.r | ( bytes.g << 8 ) | ( bytes.b << 16 ) | ( 230u << 24 );
}



const WireframeData& AddWireframePass(RenderGraph& inRenderGraph, Device& inDevice, const RenderWorld& inWorld, RenderGraphResourceID inRenderTarget, RenderGraphResourceID inDepthTarget, bool inDepthTest)
{
    return inRenderGraph.AddGraphicsPass<WireframeData>("Wireframe",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, WireframeData& inData)
    {
        inData.mRenderTarget = ioRGBuilder.RenderTarget(inRenderTarget);

        if (inDepthTest)
            inData.mDepthTarget = ioRGBuilder.DepthStencilTarget(inDepthTarget);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inRenderPass->CreatePipelineStateDesc(inDevice, g_SystemShaders.mWireframeShader);
        pso_state.InputLayout = {};
        pso_state.BlendState.RenderTarget[0].BlendEnable = true;
        pso_state.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        pso_state.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        pso_state.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso_state.RasterizerState.DepthBias = -64;
        pso_state.RasterizerState.SlopeScaledDepthBias = -2.0f;
        pso_state.DepthStencilState.DepthEnable = inDepthTest;
        pso_state.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        pso_state.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

        inData.mPipeline = inDevice.CreateGraphicsPipeline(pso_state);
        inData.mPipeline->SetName(L"PSO_WIREFRAME");
    },

    [&inRenderGraph, &inDevice, &inWorld](WireframeData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.SetViewportAndScissor(inDevice.GetTexture(inResources.GetTextureView(inData.mRenderTarget)));

        Slice<const RenderInstance> instances = inWorld.GetInstances();

        for (uint32_t instance_index = 0; instance_index < instances.size(); instance_index++)
        {
            const RenderInstance& instance = instances[instance_index];

            inCmdList.PushGraphicsConstants(WireframeRootConstants
            {
                .mInstanceIndex = instance_index,
                .mColor = sGetWireframeColor(instance.mEntity)
            });

            inCmdList.BindIndexBuffer(inDevice.GetBuffer(instance.mIndexBuffer));
            inCmdList.DrawIndexed(instance.mIndexCount, 1, 0, 0, 0);
        }
    });
}



const ComposeData& AddComposePass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inBloomTexture, RenderGraphResourceID inInputTexture)
{
    return inRenderGraph.AddGraphicsPass<ComposeData>("Compose",
    [&](RenderGraphBuilder& inBuilder, IRenderPass* inRenderPass, ComposeData& inData)
    {
        inData.mOutputTexture = inBuilder.Create(Texture::Desc
        {
            .format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .width  = inRenderGraph.GetViewport().GetDisplaySize().x,
            .height = inRenderGraph.GetViewport().GetDisplaySize().y,
            .usage  = Texture::RENDER_TARGET,
            .debugName = "RT_ComposeOutput"
        });

        inBuilder.RenderTarget(inData.mOutputTexture);

        inData.mInputTextureSRV = inBuilder.Read(inInputTexture);
        inData.mBloomTextureSRV = inBuilder.Read(inBloomTexture);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inRenderPass->CreatePipelineStateDesc(inDevice, g_SystemShaders.mFinalComposeShader);

        pso_state.InputLayout = {}; // clear the input layout, we generate the fullscreen triangle inside the vertex shader
        pso_state.DepthStencilState.DepthEnable = FALSE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inData.mPipeline = inDevice.CreateGraphicsPipeline(pso_state);
        inData.mPipeline->SetName(L"PSO_COMPOSE");
    },

    [&inRenderGraph, &inDevice](ComposeData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        //constexpr Vec4 clear_color = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        //inCmdList->ClearRenderTargetView(inDevice.GetCPUDescriptorHandle(inResources.GetTexture(inData.mOutputTexture)), glm::value_ptr(clear_color), 0, nullptr);


        
        inCmdList->SetPipelineState(inData.mPipeline.Get());

        const UVec2& display_size = inRenderGraph.GetViewport().GetDisplaySize();
        const CD3DX12_RECT scissor = CD3DX12_RECT(0, 0, display_size.x, display_size.y);
        const CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(display_size.x), float(display_size.y));

        inCmdList->RSSetViewports(1, &viewport);
        inCmdList->RSSetScissorRects(1, &scissor);

        ComposeRootConstants root_constants = ComposeRootConstants 
        {
            .mBloomTexture = inResources.GetBindlessHeapIndex(inData.mBloomTextureSRV),
            .mInputTexture = inResources.GetBindlessHeapIndex(inData.mInputTextureSRV),
            .mSettings = 
            {
                .mExposure                    = RenderSettings::GetExposure(),
                .mVignetteScale               = RenderSettings::mVignetteScale,
                .mVignetteBias                = RenderSettings::mVignetteBias,
                .mVignetteInner               = RenderSettings::mVignetteInner,
                .mVignetteOuter               = RenderSettings::mVignetteOuter,
                .mBloomBlendFactor            = RenderSettings::mBloomBlendFactor,
                .mChromaticAberrationStrength = RenderSettings::mChromaticAberrationStrength
            }
        };

        root_constants.mSettings.mVignetteScale *= g_CVariables->GetValue<int>("r_enable_vignette");
        root_constants.mSettings.mBloomBlendFactor *= g_CVariables->GetValue<int>("r_enable_bloom");

        inCmdList.PushGraphicsConstants(root_constants);
        inCmdList.Draw(3, 1, 0, 0);
    });
}



const SDFUIData& AddSDFUIPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inRenderTarget, const TextureID& inFontAtlas)
{
    return inRenderGraph.AddGraphicsPass<SDFUIData>("SDFUI",
    [&](RenderGraphBuilder& inBuilder, IRenderPass* inRenderPass, SDFUIData& inData)
    {
        inData.mPrimitivesBuffer = inBuilder.Create(Buffer::Desc
        {
            .size = sizeof(UIPrimitive) * SDFUIData::cMaxPrimitives,
            .stride = sizeof(UIPrimitive),
            .debugName = "UIPrimitivesBuffer"
        });

        inData.mPrimitivesBufferSRV = inBuilder.Read(inData.mPrimitivesBuffer);

        inBuilder.RenderTarget(inRenderTarget);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inRenderPass->CreatePipelineStateDesc(inDevice, g_SystemShaders.mSDFUIShader);
        pso_state.BlendState.RenderTarget[0].BlendEnable = true;
        pso_state.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        pso_state.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        pso_state.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        pso_state.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        pso_state.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        pso_state.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        pso_state.InputLayout = {};
        pso_state.DepthStencilState.DepthEnable = FALSE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inData.mPipeline = inDevice.CreateGraphicsPipeline(pso_state);
        inData.mPipeline->SetName(L"PSO_SDFUI");
    },

    [&inRenderGraph, &inDevice, &inFontAtlas](SDFUIData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        Slice<const UIPrimitive> primitives = g_UIRenderer.GetPrimitives();

        if (primitives.empty())
            return;

        if (primitives.size() > SDFUIData::cMaxPrimitives)
            primitives = primitives.first(SDFUIData::cMaxPrimitives);

        Buffer& primitives_buffer = inDevice.GetBuffer(inResources.GetBuffer(inData.mPrimitivesBuffer));
        inDevice.UploadBufferData(inCmdList, primitives_buffer, 0, primitives.data(), primitives.size_bytes());

        const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(primitives_buffer.GetD3D12Resource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
        inCmdList->ResourceBarrier(1, &barrier);

        inCmdList->SetPipelineState(inData.mPipeline.Get());
        inCmdList.SetViewportAndScissor(inRenderGraph.GetViewport());

        inCmdList.PushGraphicsConstants(SDFUIRootConstants
        {
            .mPrimitivesBuffer = inResources.GetBindlessHeapIndex(inData.mPrimitivesBufferSRV),
            .mFontAtlasTexture = inFontAtlas.IsValid() ? inDevice.GetBindlessHeapIndex(inFontAtlas) : 0u,
            .mFontDistanceRange = g_UIRenderer.GetFontDistanceRange(),
            .mFontAtlasPixelHeight = UIRenderer::cFontAtlasPixelHeight,
            .mInvRenderSize = 1.0f / Vec2(inRenderGraph.GetViewport().GetDisplaySize()),
        });

        inCmdList->DrawInstanced(6, uint32_t(primitives.size()), 0, 0);
    });
}


const PreImGuiData& AddPreImGuiPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID ioDisplayTexture)
{
    return inRenderGraph.AddGraphicsPass<PreImGuiData>("TRANSITION TEXTURE PASS",

    [&](RenderGraphBuilder& inBuilder, IRenderPass* inRenderPass, PreImGuiData& inData)
    {
        // this pass only exists to transition the final texture to a read state so ImGui can access it TODO: FIXME: pls fix
        inData.mDisplayTextureSRV = inBuilder.Read(ioDisplayTexture);
    },

    [&inRenderGraph, &inDevice](PreImGuiData& inData, const RenderGraphResources& inResources, CommandList& inCmdList) 
    { 
        /* nothing to do. */ 
    });
}



static void ImGui_ImplDX12_SetupRenderState(ImDrawData* draw_data, CommandList& inCmdList, Buffer& inVertexBuffer, Buffer& inIndexBuffer, ImGuiRootConstants& inRootConstants)
{
    // Setup orthographic projection matrix
    inRootConstants.mProjection = glm::ortho(
        draw_data->DisplayPos.x,
        draw_data->DisplayPos.x + draw_data->DisplaySize.x,
        draw_data->DisplayPos.y + draw_data->DisplaySize.y,
        draw_data->DisplayPos.y
    );

    // Setup viewport
    const CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, draw_data->DisplaySize.x, draw_data->DisplaySize.y);
    inCmdList->RSSetViewports(1, &viewport);

    // Bind shader and vertex buffers
    unsigned int stride = sizeof(ImDrawVert);
    unsigned int offset = 0;

    D3D12_VERTEX_BUFFER_VIEW vbv;
    memset(&vbv, 0, sizeof(D3D12_VERTEX_BUFFER_VIEW));
    vbv.BufferLocation = inVertexBuffer->GetGPUVirtualAddress() + offset;
    vbv.SizeInBytes = inVertexBuffer.GetDesc().size;
    vbv.StrideInBytes = stride;
    inCmdList->IASetVertexBuffers(0, 1, &vbv);

    D3D12_INDEX_BUFFER_VIEW ibv;
    memset(&ibv, 0, sizeof(D3D12_INDEX_BUFFER_VIEW));
    ibv.BufferLocation = inIndexBuffer->GetGPUVirtualAddress();
    ibv.SizeInBytes = inIndexBuffer.GetDesc().size;
    ibv.Format = sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    inCmdList->IASetIndexBuffer(&ibv);

    // Setup blend factor
    const float blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };
    inCmdList->OMSetBlendFactor(blend_factor);
}



const ImGuiData& AddImGuiPass(RenderGraph& inRenderGraph, Device& inDevice, RenderGraphResourceID inInputTexture, TextureID inBackBuffer)
{
    return inRenderGraph.AddGraphicsPass<ImGuiData>("ImGui",
    [&](RenderGraphBuilder& ioRGBuilder, IRenderPass* inRenderPass, ImGuiData& inData)
    {
        constexpr size_t max_buffer_size = 65536; // Taken from Vulkan's ImGuiPass
        inData.mIndexScratchBuffer.resize(max_buffer_size);
        inData.mVertexScratchBuffer.resize(max_buffer_size);

        inData.mIndexBuffer = ioRGBuilder.Create(Buffer::Desc
        {
            .size   = max_buffer_size,
            .stride = sizeof(uint16_t),
            .usage  = Buffer::Usage::INDEX_BUFFER,
            .debugName = "ImGuiIndexBuffer"
        });

        inData.mVertexBuffer = ioRGBuilder.Create(Buffer::Desc
        {
            .size   = max_buffer_size,
            .stride = sizeof(ImDrawVert),
            .usage  = Buffer::Usage::VERTEX_BUFFER,
            .debugName = "ImGuiVertexBuffer"
        });

        inData.mInputTextureSRV = ioRGBuilder.Read(inInputTexture);
        inData.mBackBufferRTV = ioRGBuilder.RenderTarget(ioRGBuilder.Import(inDevice, inBackBuffer));

        static constexpr std::array input_layout =
        {
            D3D12_INPUT_ELEMENT_DESC { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ImDrawVert, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            D3D12_INPUT_ELEMENT_DESC { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ImDrawVert, uv), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            D3D12_INPUT_ELEMENT_DESC { "COLOR", 0, DXGI_FORMAT_R32_UINT, 0, offsetof(ImDrawVert, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_state = inRenderPass->CreatePipelineStateDesc(inDevice, g_SystemShaders.mImGuiShader);
        pso_state.InputLayout = D3D12_INPUT_LAYOUT_DESC
        {
            .pInputElementDescs = input_layout.data(),
            .NumElements = input_layout.size(),
        };

        pso_state.DepthStencilState.DepthEnable = FALSE;
        pso_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        inData.mPipeline = inDevice.CreateGraphicsPipeline(pso_state);
        inData.mPipeline->SetName(L"PSO_IMGUI");
    },

    [&inRenderGraph, &inDevice, inBackBuffer](ImGuiData& inData, const RenderGraphResources& inResources, CommandList& inCmdList)
    {
        {   // manual barriers around the imported backbuffer resource, the rendergraph doesn't handle this kind of state
            auto backbuffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetD3D12Resource(inBackBuffer), D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
            inCmdList->ResourceBarrier(1, &backbuffer_barrier);
        }

        inCmdList->SetPipelineState(inData.mPipeline.Get());

        ImDrawData* draw_data = ImGui::GetDrawData();
        ImDrawIdx* idx_dst = (ImDrawIdx*)inData.mIndexScratchBuffer.data();
        ImDrawVert* vtx_dst = (ImDrawVert*)inData.mVertexScratchBuffer.data();

        for (const ImDrawList* cmd_list : draw_data->CmdLists)
        {
            memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtx_dst += cmd_list->VtxBuffer.Size;
            idx_dst += cmd_list->IdxBuffer.Size;
        }

        const int64_t index_buffer_size  = (uint8_t*)idx_dst - inData.mIndexScratchBuffer.data();
        const int64_t vertex_buffer_size = (uint8_t*)vtx_dst - inData.mVertexScratchBuffer.data();

        assert(index_buffer_size < inData.mIndexScratchBuffer.size());
        assert(vertex_buffer_size < inData.mVertexScratchBuffer.size());

        Buffer& index_buffer  = inDevice.GetBuffer(inResources.GetBuffer(inData.mIndexBuffer));
        Buffer& vertex_buffer = inDevice.GetBuffer(inResources.GetBuffer(inData.mVertexBuffer));

        int nr_of_barriers = 0;
        std::array barriers =
        {
            D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(index_buffer.GetD3D12Resource(), D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST)),
            D3D12_RESOURCE_BARRIER(CD3DX12_RESOURCE_BARRIER::Transition(vertex_buffer.GetD3D12Resource(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST))
        };

        if (index_buffer_size) nr_of_barriers++;
        if (vertex_buffer_size) nr_of_barriers++;

        if (nr_of_barriers)
            inCmdList->ResourceBarrier(nr_of_barriers, barriers.data());

        if (vertex_buffer_size)
            inDevice.UploadBufferData(inCmdList, vertex_buffer, 0, inData.mVertexScratchBuffer.data(), vertex_buffer_size);

        if (index_buffer_size)
            inDevice.UploadBufferData(inCmdList, index_buffer, 0, inData.mIndexScratchBuffer.data(), index_buffer_size);

        if (nr_of_barriers)
        {
            for (D3D12_RESOURCE_BARRIER& barrier : barriers)
                std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);

            inCmdList->ResourceBarrier(nr_of_barriers, barriers.data());
        }

        ImGuiRootConstants root_constants = { .mBindlessTextureIndex = 1 };
        ImGui_ImplDX12_SetupRenderState(draw_data, inCmdList, vertex_buffer, index_buffer, root_constants);

        int global_vtx_offset = 0;
        int global_idx_offset = 0;
        ImVec2 clip_off = draw_data->DisplayPos;

        for (const ImDrawList* cmd_list : draw_data->CmdLists)
        {
            for (const ImDrawCmd& cmd : cmd_list->CmdBuffer)
            {
                if (cmd.UserCallback)
                {
                    if (cmd.UserCallback == ImDrawCallback_ResetRenderState)
                        ImGui_ImplDX12_SetupRenderState(draw_data, inCmdList, vertex_buffer, index_buffer, root_constants);
                    else
                        cmd.UserCallback(cmd_list, &cmd);
                }
                else
                {
                    // IRenderer::GetImGuiTextureID writes out the bindless index into the ResourceDescriptorHeap, so we can assign that directly here
                    if (cmd.TextureId)
                        root_constants.mBindlessTextureIndex = 1;
                    else
                        root_constants.mBindlessTextureIndex = 1; // set 0 to make debugging easier (should be the blue noise texture I think)

                    inCmdList.PushGraphicsConstants(root_constants);

                    // Project scissor/clipping rectangles into framebuffer space
                    const ImVec2 clip_min = ImVec2(cmd.ClipRect.x - clip_off.x, cmd.ClipRect.y - clip_off.y);
                    const ImVec2 clip_max = ImVec2(cmd.ClipRect.z - clip_off.x, cmd.ClipRect.w - clip_off.y);
                    if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                        continue;

                    const D3D12_RECT scissor_rect = D3D12_RECT { (LONG)clip_min.x, (LONG)clip_min.y, (LONG)clip_max.x, (LONG)clip_max.y };
                    inCmdList->RSSetScissorRects(1, &scissor_rect);

                    inCmdList->DrawIndexedInstanced(cmd.ElemCount, 1, cmd.IdxOffset + global_idx_offset, cmd.VtxOffset + global_vtx_offset, 0);
                }
            }

            global_idx_offset += cmd_list->IdxBuffer.Size;
            global_vtx_offset += cmd_list->VtxBuffer.Size;
        }

        {
            auto backbuffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(inDevice.GetD3D12Resource(inBackBuffer), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT | D3D12_RESOURCE_STATE_COMMON);
            inCmdList->ResourceBarrier(1, &backbuffer_barrier);
        }
    });
}

} // raekor