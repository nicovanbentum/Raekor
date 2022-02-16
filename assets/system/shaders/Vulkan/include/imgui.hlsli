struct VS_INPUT {
    float2 pos : POSITION;
    float2 uv : TEXCOORD;
    uint color : COLOR;
};

struct VS_OUTPUT {
    float2 uv : TEXCOORD0;
    float4 color :COLOR;
};

struct PushConstants {
    float4x4 proj;
    int textureIndex;
};