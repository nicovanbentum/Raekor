#version 440 core

layout (triangles) in;
layout (triangle_strip, max_vertices=3) out;

in vec3 positions[];
in vec3 normals[];
in vec2 uvs[];

out vec3 f_position;
out vec3 f_normal;
out vec2 f_uv;

void main() {
	// get triangle normal
	vec3 face_normal = normalize(normals[0] + normals[1] + normals[2]);
	vec3 n = abs(face_normal);

	for(uint i = 0; i < 3; ++i) {
		f_position = positions[i];
		f_normal = normals[i];
		f_uv = uvs[i];

		// dominant axis selection
		if(n.z > n.x && n.z > n.y) {
			gl_Position = vec4(f_position.x, f_position.y, 0, 1);
		}
		else if(n.x > n.y && n.x > n.z) {
			gl_Position = vec4(f_position.y, f_position.z, 0, 1);
		}
		else {
			gl_Position = vec4(f_position.x, f_position.z, 0, 1);
		}

		EmitVertex();
	}

	EndPrimitive();
}