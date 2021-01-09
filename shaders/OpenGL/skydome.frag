#version 450

layout(location = 0) out vec4 final_color;

in float lerp;

uniform vec3 mid_color;
uniform vec3 top_color;

in VS_OUT {
    float lerp;
} fs_in;

void main() {
    float lerp = fs_in.lerp;
    if(lerp < 0.0f) final_color.rgb = mix(vec3(0,0,0), mid_color, 1 - -lerp);
    else final_color.rgb = mix(mid_color, top_color, lerp);
    final_color.a = 1.0;
}