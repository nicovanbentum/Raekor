#pragma once

#include "json.h"
#include "components.h"
#include "primitives.h"

namespace Raekor {

}

namespace cereal {

constexpr auto xyzw = std::array { "x", "y", "z", "w" };

template<class Archive, glm::length_t L, typename T>
void serialize(Archive& archive, glm::vec<L, T>& v) {
	for (glm::length_t i = 0; i < v.length(); i++)
		archive(cereal::make_nvp(xyzw[i], v[i]));
}


template<class Archive, glm::length_t C, glm::length_t R, typename T>
void serialize(Archive& archive, glm::mat<C, R, T>& mat) {
	for (glm::length_t i = 0; i < mat.length(); i++)
		archive(cereal::make_nvp(xyzw[i % xyzw.size()], mat[i]));
}


template<class Archive, typename T>
void serialize(Archive& archive, glm::qua<T>& quat) {
	for (glm::length_t i = 0; i < quat.length(); i++)
		archive(cereal::make_nvp(xyzw[i], quat[i]));
}


template<typename Archive>
void save(Archive& archive, const Raekor::Transform& transform) {
	archive(transform.position, transform.rotation, transform.scale, 
		transform.localTransform, transform.worldTransform);
}

template<typename Archive>
void load(Archive& archive, Raekor::Transform& transform) {
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
void serialize(Archive& archive, Raekor::PointLight& light) {
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
	archive(v.pos, v.uv, v.normal, v.tangent);
}


template<class Archive>
void save(Archive& archive, const Raekor::Mesh& mesh) {
	archive(
		cereal::make_nvp("positions", mesh.positions),
		cereal::make_nvp("uvs", mesh.uvs),
		cereal::make_nvp("normals", mesh.normals),
		cereal::make_nvp("tangents", mesh.tangents),
		cereal::make_nvp("indices", mesh.indices),
		cereal::make_nvp("material", mesh.material)
	);
}


template<class Archive>
void load(Archive& archive, Raekor::Mesh& mesh) {
	archive(
		cereal::make_nvp("positions", mesh.positions),
		cereal::make_nvp("uvs", mesh.uvs),
		cereal::make_nvp("normals", mesh.normals),
		cereal::make_nvp("tangents", mesh.tangents),
		cereal::make_nvp("indices", mesh.indices),
		cereal::make_nvp("material", mesh.material)
	);
}

template<class Archive>
void serialize(Archive& archive, JPH::Vec3& vec) {
	archive(vec.mF32);
}

template<class Archive>
void serialize(Archive& archive, JPH::BoxShapeSettings& settings) {
	archive(settings.mHalfExtent, settings.mConvexRadius);
}

template<class Archive>
void serialize(Archive& archive, Raekor::BoxCollider& collider) {
	archive(collider.motionType, collider.settings);
}


template<class Archive>
void save(Archive& archive, const Raekor::Material& mat) {
	archive(
		cereal::make_nvp("Albedo Map", mat.albedoFile),
		cereal::make_nvp("Normal Map", mat.normalFile),
		cereal::make_nvp("Metallic-Roughness Map", mat.metalroughFile),
		cereal::make_nvp("Base Color", mat.albedo),
		cereal::make_nvp("Metallic", mat.metallic),
		cereal::make_nvp("Roughness", mat.roughness),
		cereal::make_nvp("emissive", mat.emissive),
		cereal::make_nvp("Transparent", mat.isTransparent)
	);
}


template<class Archive>
void load(Archive& archive, Raekor::Material& mat) {
	archive(
		cereal::make_nvp("Albedo Map", mat.albedoFile),
		cereal::make_nvp("Normal Map", mat.normalFile),
		cereal::make_nvp("Metallic-Roughness Map", mat.metalroughFile),
		cereal::make_nvp("Base Color", mat.albedo),
		cereal::make_nvp("Metallic", mat.metallic),
		cereal::make_nvp("Roughness", mat.roughness),
		cereal::make_nvp("emissive", mat.emissive),
		cereal::make_nvp("Transparent", mat.isTransparent)
	);
}


template<class Archive>
void serialize(Archive& archive, Raekor::Name& name) {
	archive(cereal::make_nvp("name", name.name));
}


template<class Archive>
void load(Archive& archive, std::function<void()>& fn) {}


template<class Archive>
void save(Archive& archive, const std::function<void()>& fn) {}

} // namespace cereal
