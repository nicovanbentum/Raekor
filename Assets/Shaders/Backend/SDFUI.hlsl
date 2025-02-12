#include "Include/Bindless.hlsli"
#include "Include/Packing.hlsli"
#include "Include/Common.hlsli"

FRAME_CONSTANTS(fc)
ROOT_CONSTANTS(SDFUIRootConstants, rc)

float4 main(in FULLSCREEN_TRIANGLE_VS_OUT inParams) : SV_Target0
{
    StructuredBuffer<float4> draw_command_buffer = ResourceDescriptorHeap[rc.mDrawCommandBuffer];
    StructuredBuffer<DrawCommandHeader> draw_header_buffer = ResourceDescriptorHeap[rc.mDrawCommandHeaderBuffer];
    
    float4 final_color = float4(0, 0, 0, 0);
    
    for (int command_index = 0; command_index < rc.mCommandCount; command_index++)
    {
        DrawCommandHeader header = draw_header_buffer[command_index];
        
        if (header.type == DRAW_COMMAND_CIRCLE_FILLED)
        {
            uint offset       = header.startOffset;
            float4 color      = draw_command_buffer[offset++];
            float4 pos_radius = draw_command_buffer[offset++];
            
            float distance = length(inParams.mPixelCoords.xy - pos_radius.xy) - pos_radius.z;
            
            //float glow_strength = 0.5;
            //float glow_mask = saturate(distance / 20.0f);
            //final_color = lerp(final_color, float4(1, 1, 0, 1), (1.0 - glow_mask) * glow_strength);
            
            final_color = lerp(final_color, color, saturate(1.0 - distance));
        }
        else if (header.type == DRAW_COMMAND_RECT)
        {
            uint offset     = header.startOffset;
            float4 color    = draw_command_buffer[offset++];
            float4 pos_size = draw_command_buffer[offset++];
            float radius    = draw_command_buffer[offset++].x;
            
            float distance = length(max(abs(inParams.mPixelCoords.xy - pos_size.xy) - pos_size.zw + radius, 0.0)) - radius;
            final_color = lerp(final_color, color, saturate(1.0 - distance));
        }
    }
    
    return final_color;

}