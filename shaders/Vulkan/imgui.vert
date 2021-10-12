#version 460

layout(location = 0) in vec2 v_pos;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in uint v_col;

layout(push_constant) uniform pushConstants {
    mat4 proj;
    int textureIndex;
};

layout(location = 0) out vec2 o_uv;
layout(location = 1) out vec4 o_col;

void main() 
{
	gl_Position = proj * vec4(v_pos, 0.0, 1.0);
    gl_Position.z = (gl_Position.z / gl_Position.w * 0.5 + 0.5) * gl_Position.w;
    o_uv = v_uv;

	// a_color is ABGR
	o_col.w = float((v_col & uint(0xff000000)) >> 24) / 255;
	o_col.z = float((v_col & uint(0x00ff0000)) >> 16) / 255;
	o_col.y = float((v_col & uint(0x0000ff00)) >> 8) / 255;
	o_col.x = float((v_col & uint(0x000000ff)) >> 0) / 255;
}