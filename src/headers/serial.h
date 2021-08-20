#pragma once
#include "components.h"

namespace cereal {

template<class Archive> void serialize(Archive& archive, glm::vec2& v) { archive(v.x, v.y); }
template<class Archive> void serialize(Archive& archive, glm::vec3& v) { archive(v.x, v.y, v.z); }
template<class Archive> void serialize(Archive& archive, glm::vec4& v) { archive(v.x, v.y, v.z, v.w); }
template<class Archive> void serialize(Archive& archive, glm::ivec2& v) { archive(v.x, v.y); }
template<class Archive> void serialize(Archive& archive, glm::ivec3& v) { archive(v.x, v.y, v.z); }
template<class Archive> void serialize(Archive& archive, glm::ivec4& v) { archive(v.x, v.y, v.z, v.w); }
template<class Archive> void serialize(Archive& archive, glm::uvec2& v) { archive(v.x, v.y); }
template<class Archive> void serialize(Archive& archive, glm::uvec3& v) { archive(v.x, v.y, v.z); }
template<class Archive> void serialize(Archive& archive, glm::uvec4& v) { archive(v.x, v.y, v.z, v.w); }
template<class Archive> void serialize(Archive& archive, glm::dvec2& v) { archive(v.x, v.y); }
template<class Archive> void serialize(Archive& archive, glm::dvec3& v) { archive(v.x, v.y, v.z); }
template<class Archive> void serialize(Archive& archive, glm::dvec4& v) { archive(v.x, v.y, v.z, v.w); }
template<class Archive> void serialize(Archive& archive, glm::mat2& m) { archive(m[0], m[1]); }
template<class Archive> void serialize(Archive& archive, glm::dmat2& m) { archive(m[0], m[1]); }
template<class Archive> void serialize(Archive& archive, glm::mat3& m) { archive(m[0], m[1], m[2]); }
template<class Archive> void serialize(Archive& archive, glm::mat4& m) { archive(m[0], m[1], m[2], m[3]); }
template<class Archive> void serialize(Archive& archive, glm::dmat4& m) { archive(m[0], m[1], m[2], m[3]); }
template<class Archive> void serialize(Archive& archive, glm::quat& q) { archive(q.x, q.y, q.z, q.w); }
template<class Archive> void serialize(Archive& archive, glm::dquat& q) { archive(q.x, q.y, q.z, q.w); }

//////////////////////////////////////////////////////////////////////////////////////////////////

template<typename Archive>
void serialize(Archive& archive, Raekor::Transform& transform) {
	archive(transform.position, transform.rotation, transform.scale, 
		transform.localTransform, transform.worldTransform);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

template<typename Archive> 
void save(Archive& archive, const Raekor::DirectionalLight& light) {
	archive(light.colour, light.direction);
}

template<typename Archive>
void load(Archive& archive, Raekor::DirectionalLight& light) {
	archive(light.colour, light.direction);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

template<typename Archive>
void serialize(Archive& archive, Raekor::PointLight light) {
	archive(light.colour, light.colour);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

template<class Archive>
void serialize(Archive& archive, Raekor::Node& node) {
    archive(node.parent, node.firstChild, node.nextSibling, node.prevSibling);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

template<class Archive>
void serialize(Archive& archive, Raekor::BoneInfo& boneInfo) {
	archive(boneInfo.boneOffset, boneInfo.finalTransformation);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

template<class Archive>
void serialize(Archive& archive, Raekor::Triangle& tri) {
	archive(tri.p1, tri.p2, tri.p3);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

template<class Archive>
void serialize(Archive& archive, Raekor::Vertex& v) {
	archive(v.pos, v.uv, v.normal, v.tangent, v.binormal);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

template<class Archive>
void save(Archive& archive, const Raekor::Mesh& mesh) {
	archive(mesh.positions, mesh.uvs, mesh.normals, mesh.tangents, mesh.indices, mesh.material);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

template<class Archive>
void load(Archive& archive, Raekor::Mesh& mesh) {
	archive(mesh.positions, mesh.uvs, mesh.normals, mesh.tangents, mesh.indices, mesh.material);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

template<class Archive>
void save(Archive& archive, const Raekor::Material& mat) {
	archive(mat.albedoFile, mat.normalFile, mat.mrFile, mat.baseColour, mat.metallic, mat.roughness);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

template<class Archive>
void load(Archive& archive, Raekor::Material& mat) {
	archive(mat.albedoFile, mat.normalFile, mat.mrFile, mat.baseColour, mat.metallic, mat.roughness);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

template<class Archive>
void serialize(Archive& archive, Raekor::Name& name) {
    archive(name.name);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

template<class Archive>
void load(Archive& archive, std::function<void()>& fn) {

}

//////////////////////////////////////////////////////////////////////////////////////////////////

template<class Archive>
void save(Archive& archive, const std::function<void()>& fn) {

}

} // namespace cereal
