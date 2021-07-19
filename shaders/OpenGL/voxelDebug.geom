#version 440 core

layout(points) in;
layout(triangle_strip, max_vertices = 36) out;

layout(binding = 0) uniform ubo {
    mat4 p;
    mat4 mv;
    vec4 cameraPosition;
};

in vec4 color[];

out vec4 fragColor;

void main() {
	fragColor = color[0];

    if(fragColor.a < 0.5f) return;

	vec4 v1 = p * mv * (gl_in[0].gl_Position + vec4(-0.5, 0.5, 0.5, 0));
	vec4 v2 = p * mv * (gl_in[0].gl_Position + vec4(0.5, 0.5, 0.5, 0));
	vec4 v3 = p * mv * (gl_in[0].gl_Position + vec4(-0.5, -0.5, 0.5, 0));
	vec4 v4 = p * mv * (gl_in[0].gl_Position + vec4(0.5, -0.5, 0.5, 0));
	vec4 v5 = p * mv * (gl_in[0].gl_Position + vec4(-0.5, 0.5, -0.5, 0));
	vec4 v6 = p * mv * (gl_in[0].gl_Position + vec4(0.5, 0.5, -0.5, 0));
	vec4 v7 = p * mv * (gl_in[0].gl_Position + vec4(-0.5, -0.5, -0.5, 0));
	vec4 v8 = p * mv * (gl_in[0].gl_Position + vec4(0.5, -0.5, -0.5, 0));

    // expand to cube
	//
	//      v5 _____________ v6
	//        /|           /|
	//       / |          / |
	//      /  |         /  |
	//     /   |        /   |
	// v1 /____|_______/ v2 |
	//    |    |       |    |
	//    |    |_v7____|____| v8
	//    |   /        |   /
	//    |  /         |  /  
	//    | /          | /  
	// v3 |/___________|/ v4
	//

	// TODO: Optimize
	// +Z
    gl_Position = v1;
    EmitVertex();
    gl_Position = v3;
    EmitVertex();
	gl_Position = v4;
    EmitVertex();
    EndPrimitive();
    gl_Position = v1;
    EmitVertex();
    gl_Position = v4;
    EmitVertex();
	gl_Position = v2;
    EmitVertex();
    EndPrimitive();

    // -Z
    gl_Position = v6;
    EmitVertex();
    gl_Position = v8;
    EmitVertex();
	gl_Position = v7;
    EmitVertex();
    EndPrimitive();
    gl_Position = v6;
    EmitVertex();
    gl_Position = v7;
    EmitVertex();
	gl_Position = v5;
    EmitVertex();
    EndPrimitive();

    // +X
    gl_Position = v2;
    EmitVertex();
    gl_Position = v4;
    EmitVertex();
	gl_Position = v8;
    EmitVertex();
    EndPrimitive();
    gl_Position = v2;
    EmitVertex();
    gl_Position = v8;
    EmitVertex();
	gl_Position = v6;
    EmitVertex();
    EndPrimitive();

    // -X
    gl_Position = v5;
    EmitVertex();
    gl_Position = v7;
    EmitVertex();
	gl_Position = v3;
    EmitVertex();
    EndPrimitive();
    gl_Position = v5;
    EmitVertex();
    gl_Position = v3;
    EmitVertex();
	gl_Position = v1;
    EmitVertex();
    EndPrimitive();

    // +Y
    gl_Position = v5;
    EmitVertex();
    gl_Position = v1;
    EmitVertex();
	gl_Position = v2;
    EmitVertex();
    EndPrimitive();
    gl_Position = v5;
    EmitVertex();
    gl_Position = v2;
    EmitVertex();
	gl_Position = v6;
    EmitVertex();
    EndPrimitive();

    // -Y
    gl_Position = v3;
    EmitVertex();
    gl_Position = v7;
    EmitVertex();
	gl_Position = v8;
    EmitVertex();
    EndPrimitive();
    gl_Position = v3;
    EmitVertex();
    gl_Position = v8;
    EmitVertex();
	gl_Position = v4;
    EmitVertex();
    EndPrimitive();
}