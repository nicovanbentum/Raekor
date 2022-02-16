#version 440 core

layout (triangles) in;
layout (triangle_strip, max_vertices=3) out;

layout(location = 0) in vec2 uvs[];
layout(location = 1) in vec4 worldPositions[];

layout(location = 0) out GEOM_OUT {
    vec2 uv;
    flat int axis;
    vec4 worldPosition;
    vec4 normal;
};

layout(binding = 0) uniform ubo {
    mat4 px, py, pz;
    mat4 shadowMatrices[4];
    mat4 view;
    mat4 model;
    vec4 shadowSplits;
    vec4 colour;
};

void main() {
    // get triangle normal
    vec3 p1 = gl_in[1].gl_Position.xyz - gl_in[0].gl_Position.xyz;
    vec3 p2 = gl_in[2].gl_Position.xyz - gl_in[0].gl_Position.xyz;
    normal = vec4(normalize(cross(p1,p2)), 1.0);

    float nDotX = abs(normal.x);
    float nDotY = abs(normal.y);
    float nDotZ = abs(normal.z);

    // 0 = x axis dominant, 1 = y axis dominant, 2 = z axis dominant
    axis = (nDotX >= nDotY && nDotX >= nDotZ) ? 1 : (nDotY >= nDotX && nDotY >= nDotZ) ? 2 : 3;
    mat4 p = axis == 1 ? px : axis == 2 ? py : pz;
    
    for(int i = 0; i < gl_in.length(); i++) {
        uv = uvs[i];
        worldPosition = worldPositions[i];
        gl_Position = p * gl_in[i].gl_Position;
        EmitVertex();
    }
    
    // Finished creating vertices
    EndPrimitive();
}