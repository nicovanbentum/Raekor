#pragma once

namespace Raekor::DX12 {

enum EUpscaler
{
    UPSCALER_NONE,
    UPSCALER_FSR,
    UPSCALER_DLSS,
    UPSCALER_XESS,
    UPSCALER_COUNT
};


enum EUpscalerQuality
{
    UPSCALER_QUALITY_NATIVE,
    UPSCALER_QUALITY_QUALITY,
    UPSCALER_QUALITY_BALANCED,
    UPSCALER_QUALITY_PERFORMANCE,
    UPSCALER_QUALITY_COUNT
};


class Upscaler
{
public:
    struct InitParams
    {
        ID3D12Device* mDevice;
    };

    struct DispatchParams
    {
        ID3D12CommandList*  mCommandList;                        
        ID3D12Resource*     mColorTexture;                              
        ID3D12Resource*     mDepthTexture;                              
        ID3D12Resource*     mVelocityTexture;
        ID3D12Resource*     mOutputTexture;
        float               mJitterOffsetX;
        float               mJitterOffsetY;
        float               mMotionVectorScaleX;
        float               mMotionVectorScaleY;
        uint32_t            mRenderSizeX;
        uint32_t            mRenderSizeY;
        bool                mEnableSharpening;                   
        float               mSharpness;                          
        float               mDeltaTime;                     
        float               mPreExposure;                        
        bool                mCameraCut;                              
        float               mCameraNear;                         
        float               mCameraFar;                          
        float               mCameraFovAngleVertical;             
    };

    static UVec2 sGetRenderResolution(UVec2 inDisplayResolution, EUpscalerQuality inQuality);
    static NVSDK_NGX_PerfQuality_Value sGetQuality(EUpscalerQuality inQuality);

    bool Init(const InitParams& inParams);
    bool Destroy(ID3D12Device* inDevice);
    void Dispatch(const DispatchParams& inParams);

private:
    bool InitFSR(const InitParams& inParams);
    bool InitDLSS(const InitParams& inParams);
    bool InitXeSS(const InitParams& inParams);

    bool DestroyFSR(ID3D12Device* inDevice);
    bool DestroyDLSS(ID3D12Device* inDevice);
    bool DestroyXeSS(ID3D12Device* inDevice);

    void DispatchFSR(const DispatchParams& inParams);
    void DispatchDLSS(const DispatchParams& inParams);
    void DispatchXeSS(const DispatchParams& inParams);
};


} // namespace raekor