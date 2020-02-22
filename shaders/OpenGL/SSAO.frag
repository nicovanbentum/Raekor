#version 440 core

in vec2 uv;
in vec2 ray;

out vec4 FragColor;

layout(binding = 1) uniform sampler2D gDepthMap;
uniform float gSampleRad;
uniform mat4 gProj;

const int MAX_KERNEL_SIZE = 64;
uniform vec3 gKernel[MAX_KERNEL_SIZE];


float CalcViewZ(vec2 Coords) {
    float Depth = texture(gDepthMap, Coords).x;
	mat4 proj = transpose(gProj);
    float ViewZ = proj[3][2] / (2 * Depth -1 - proj[2][2]);
    return ViewZ;
}


void main()
{
    float ViewZ = CalcViewZ(uv);

    float ViewX = ray.x * ViewZ;
    float ViewY = ray.y * ViewZ;

    vec3 Pos = vec3(ViewX, ViewY, ViewZ);

    float AO = 0.0;

    for (int i = 0 ; i < MAX_KERNEL_SIZE ; i++) {
        vec3 samplePos = Pos + gKernel[i];
        vec4 offset = vec4(samplePos, 1.0);
        offset = gProj * offset;
        offset.xy /= offset.w;
        offset.xy = offset.xy * 0.5 + vec2(0.5);

        float sampleDepth = CalcViewZ(offset.xy);

        if (abs(Pos.z - sampleDepth) < gSampleRad) {
            AO += step(sampleDepth,samplePos.z);
        }
    }

    AO = 1.0 - AO/64.0;

    FragColor = vec4(pow(AO, 2.0));
}