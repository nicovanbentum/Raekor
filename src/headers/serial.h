#pragma once
#include "components.h"

namespace cereal {
    // GLM TYPES
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


    template<typename Archive>
    void serialize(Archive& archive, Raekor::ECS::TransformComponent& transform) {
		archive(
			transform.position,
			transform.rotation,
			transform.scale,
			transform.matrix,
			transform.worldTransform,
			transform.localPosition
		);
    }

	template<typename Archive> 
	void serialize(Archive& archive, Raekor::ECS::DirectionalLightComponent) {}

	template<typename Archive>
	void serialize(Archive& archive, Raekor::ECS::PointLightComponent) {}

    template<class Archive>
    void serialize(Archive& archive, Raekor::ECS::NodeComponent& node) {
        archive(node.parent, node.hasChildren);
    }

	template<class Archive>
	void serialize(Archive& archive, Raekor::ECS::BoneInfo& boneInfo) {
		archive(boneInfo.boneOffset, boneInfo.finalTransformation);
	}

	template<class Archive>
	void serialize(Archive& archive, Raekor::Triangle& tri) {
		archive(tri.p1, tri.p2, tri.p3);
	}

	template<class Archive>
	void serialize(Archive& archive, Raekor::Vertex& v) {
		archive(
			v.pos, v.uv, v.normal, v.tangent, v.binormal
		);
	}

	template<class Archive>
	void save(Archive& archive, const Raekor::ECS::MeshComponent& mesh) {
		archive(mesh.vertices, mesh.indices);
	}

	template<class Archive>
	void load(Archive& archive, Raekor::ECS::MeshComponent& mesh) {
		archive(mesh.vertices, mesh.indices);
	}

	template<class Archive>
	void save(Archive& archive, const Raekor::ECS::MaterialComponent& mat) {
		archive(mat.albedoFile, mat.normalFile, mat.mrFile);
	}

	template<class Archive>
	void load(Archive& archive, Raekor::ECS::MaterialComponent& mat) {
		archive(mat.albedoFile, mat.normalFile, mat.mrFile);
	}

    template<class Archive>
    void serialize(Archive& archive, Raekor::ECS::NameComponent& name) {
        archive(name.name);
    }
}
