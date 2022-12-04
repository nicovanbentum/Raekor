#ifdef __cplusplus
    using uint      = uint32_t;
    using float2    = glm::vec2;
    using float3    = glm::vec3;
    using float4    = glm::vec4;
    using float3x3 =  glm::mat3;
    using float4x4  = glm::mat4;
#endif
    
struct FrameConstants {
    float4 mSunDirection;
    float4 mCameraPosition;
    float4x4 mViewMatrix;
    float4x4 mProjectionMatrix;
    float4x4 mInvViewProjectionMatrix;
};