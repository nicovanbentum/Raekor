#pragma once

#include "components.h"
#include "primitives.h"

namespace cereal {

template<class Archive, glm::length_t L, typename T>
void serialize(Archive& archive, glm::vec<L, T>& v) {
	for (glm::length_t i = 0; i < v.length(); i++) {
		archive(v[i]);
	}
}


template<class Archive, glm::length_t C, glm::length_t R, typename T>
void serialize(Archive& archive, glm::mat<C, R, T>& mat) {
	for (glm::length_t i = 0; i < mat.length(); i++) {
		archive(mat[i]);
	}
}


template<class Archive, typename T>
void serialize(Archive& archive, glm::qua<T>& quat) {
	for (glm::length_t i = 0; i < quat.length(); i++) {
		archive(quat[i]);
	}
}


template<typename Archive>
void serialize(Archive& archive, Raekor::Transform& transform) {
	archive(transform.position, transform.rotation, transform.scale, 
		transform.localTransform, transform.worldTransform);
}


template<typename Archive> 
void save(Archive& archive, const Raekor::DirectionalLight& light) {
	archive(light.colour, light.direction);
}


template<typename Archive>
void load(Archive& archive, Raekor::DirectionalLight& light) {
	archive(light.colour, light.direction);
}


template<typename Archive>
void serialize(Archive& archive, Raekor::PointLight light) {
	archive(light.colour, light.colour);
}


template<class Archive>
void serialize(Archive& archive, Raekor::Node& node) {
    archive(node.parent, node.firstChild, node.nextSibling, node.prevSibling);
}


template<class Archive>
void serialize(Archive& archive, Raekor::Triangle& tri) {
	archive(tri.p1, tri.p2, tri.p3);
}


template<class Archive>
void serialize(Archive& archive, Raekor::Vertex& v) {
	archive(v.pos, v.uv, v.normal, v.tangent, v.binormal);
}


template<class Archive>
void save(Archive& archive, const Raekor::Mesh& mesh) {
	archive(mesh.positions, mesh.uvs, mesh.normals, mesh.tangents, mesh.indices, mesh.material);
}


template<class Archive>
void load(Archive& archive, Raekor::Mesh& mesh) {
	archive(mesh.positions, mesh.uvs, mesh.normals, mesh.tangents, mesh.indices, mesh.material);
}


template<class Archive>
void save(Archive& archive, const Raekor::Material& mat) {
	archive(mat.albedoFile, mat.normalFile, mat.metalroughFile, mat.albedo, mat.metallic, mat.roughness, mat.emissive);
}


template<class Archive>
void load(Archive& archive, Raekor::Material& mat) {
	archive(mat.albedoFile, mat.normalFile, mat.metalroughFile, mat.albedo, mat.metallic, mat.roughness, mat.emissive);
}


template<class Archive>
void serialize(Archive& archive, Raekor::Name& name) {
    archive(name.name);
}


template<class Archive>
void load(Archive& archive, std::function<void()>& fn) {}


template<class Archive>
void save(Archive& archive, const std::function<void()>& fn) {}

} // namespace cereal
