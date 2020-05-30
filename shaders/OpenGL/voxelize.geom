#version 440 core

layout (triangles) in;
layout (triangle_strip, max_vertices=3) out;

in vec2 uvs[];
in vec4 depthPositions[];

out vec2 uv;
out mat4 p;
out flat int axis;
out vec4 depthPosition;

uniform mat4 px;
uniform mat4 py;
uniform mat4 pz;

void main() {
    vec3 p1 = gl_in[1].gl_Position.xyz - gl_in[0].gl_Position.xyz;
    vec3 p2 = gl_in[2].gl_Position.xyz - gl_in[0].gl_Position.xyz;
    vec3 normal = normalize(cross(p1,p2));

    float nDotX = abs(normal.x);
    float nDotY = abs(normal.y);
    float nDotZ = abs(normal.z);

    // 0 = x axis dominant, 1 = y axis dominant, 2 = z axis dominant
    axis = (nDotX >= nDotY && nDotX >= nDotZ) ? 1 : (nDotY >= nDotX && nDotY >= nDotZ) ? 2 : 3;
    p = axis == 1 ? px : axis == 2 ? py : pz;
    
    for(int i = 0; i < gl_in.length(); i++) {
        uv = uvs[i];
        depthPosition = depthPositions[i];
        gl_Position = p * gl_in[i].gl_Position;
        EmitVertex();
    }
    
    // Finished creating vertices
    EndPrimitive();
}