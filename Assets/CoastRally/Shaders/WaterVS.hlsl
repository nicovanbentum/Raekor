#include "Include/Common.hlsli"
#include "Include/Random.hlsli"

struct VS_OUTPUT {
    float4 sv_position      : SV_Position;
    float4 curr_position    : POS0;
    float4 prev_position    : POS1;
    float2 texcoord         : TEXCOORD;
    float3 normal           : NORMAL;
    float3 tangent          : TANGENT;
    float3 bitangent        : BINORMAL;
    nointerpolation uint instance_index : INSTANCE_INDEX;
};

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(GbufferRootConstants, rc)

void Procedure3(float3 inPosition, float inTime, float inAmplitude, out float3 outPosition, out float3 outNormal)
{
 const float4 waves[4] =
{
    float4(0.86, 0.50, 42.0, 0.45),
    float4(-0.38, 0.92, 27.0, 0.30),
    float4(0.64, -0.77, 19.0, 0.18),
    float4(-0.93, -0.36, 16.0, 0.10)
};

const float fade = 1.0 - smoothstep(280.0, 380.0, length(inPosition.xz));

float3 offset = float3(0.0, 0.0, 0.0);
float3 tangent_x = float3(1.0, 0.0, 0.0);
float3 tangent_z = float3(0.0, 0.0, 1.0);

[unroll]
for (int index = 0; index < 4; index++)
{
    const float2 direction = normalize(waves[index].xy);
    const float k = 6.2831853 / waves[index].z;
    const float speed = sqrt(9.81 / k);
    const float height = inAmplitude * waves[index].w * fade;
    const float sway = height * 0.6;
    const float phase = k * (dot(direction, inPosition.xz) - speed * inTime);
    const float s = sin(phase);
    const float c = cos(phase);

    offset += float3(direction.x * sway * c, height * s, direction.y * sway * c);

    tangent_x += float3(-direction.x * direction.x * sway * k * s, direction.x * height * k * c, -direction.x * direction.y * sway * k * s);
    tangent_z += float3(-direction.x * direction.y * sway * k * s, direction.y * height * k * c, -direction.y * direction.y * sway * k * s);
}

outPosition = inPosition + offset;
outNormal = normalize(cross(tangent_z, tangent_x)); 
}



VS_OUTPUT main(in uint inVertexID : SV_VertexID, in uint inInstanceID : SV_InstanceID)
{
    const uint instance_index = rc.mInstanceIndex + inInstanceID;

    StructuredBuffer<RTGeometry> geometries = ResourceDescriptorHeap[fc.mInstancesBuffer];
    RTGeometry geometry = geometries[instance_index];

    StructuredBuffer<RTVertex> vertex_buffer = ResourceDescriptorHeap[geometry.mVertexBuffer];

    RTVertex vertex = vertex_buffer[inVertexID];

    // Node 3 -> FloatValueShaderNode
    float FloatValue_Out0 = 0.800;
    // Node 4 -> FloatOpShaderNode
    float FloatOp_Out1 = float(fc.mTime * FloatValue_Out0);
    // Node 5 -> FloatValueShaderNode
    float FloatValue_Out2 = 0.400;
    // Node 6 -> ProcedureShaderNode
    float3 outPosition3;
    float3 outNormal4;
    Procedure3(vertex.mPos, FloatOp_Out1, FloatValue_Out2, outPosition3, outNormal4);
    // Node 0 -> VertexShaderOutputShaderNode
    vertex.mPos = outPosition3;
    vertex.mNormal = outNormal4;


    float4 prev_pos_ws = mul(geometry.mPrevWorldTransform, float4(vertex.mPos, 1.0));

    TransformToWorldSpace(vertex, geometry.mWorldTransform);
    float4 curr_pos_ws = float4(vertex.mPos, 1.0);

    VS_OUTPUT output;
    output.normal = normalize(vertex.mNormal);
    output.tangent = normalize(vertex.mTangent);
    output.tangent = normalize(output.tangent - dot(output.tangent, output.normal) * output.normal);
    output.bitangent = normalize(cross(output.normal, output.tangent));
    output.texcoord = vertex.mTexCoord;

    output.curr_position = mul(fc.mViewProjectionMatrix, curr_pos_ws);
    output.prev_position = mul(fc.mPrevViewProjectionMatrix, prev_pos_ws);
    output.sv_position = output.curr_position;
    output.instance_index = instance_index;
    return output;
}
