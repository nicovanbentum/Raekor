#include "pch.h"
#include "components.h"
#include "timer.h"
#include "assets.h"
#include "primitives.h"

namespace Raekor {

RTTI_DEFINE_TYPE(Name)
{
	RTTI_DEFINE_MEMBER(Name, SERIALIZE_ALL, "Name", name);
}


RTTI_DEFINE_TYPE(Transform)
{
	RTTI_DEFINE_MEMBER(Transform, SERIALIZE_ALL, "Scale", scale);
	RTTI_DEFINE_MEMBER(Transform, SERIALIZE_ALL, "Position", position);
	RTTI_DEFINE_MEMBER(Transform, SERIALIZE_ALL, "Rotation", rotation);
	RTTI_DEFINE_MEMBER(Transform, SERIALIZE_ALL, "Local Transform", localTransform);
	RTTI_DEFINE_MEMBER(Transform, SERIALIZE_ALL, "World Transform", worldTransform);
}


RTTI_DEFINE_TYPE(DirectionalLight)
{
	RTTI_DEFINE_MEMBER(DirectionalLight, SERIALIZE_ALL, "Direction", direction);
	RTTI_DEFINE_MEMBER(DirectionalLight, SERIALIZE_ALL, "Color", colour);
}


RTTI_DEFINE_TYPE(PointLight)
{
	RTTI_DEFINE_MEMBER(PointLight, SERIALIZE_ALL, "Position", position);
	RTTI_DEFINE_MEMBER(PointLight, SERIALIZE_ALL, "Color", colour);
}


RTTI_DEFINE_TYPE(Node)
{
	RTTI_DEFINE_MEMBER(Node, SERIALIZE_ALL, "Parent", parent);
	RTTI_DEFINE_MEMBER(Node, SERIALIZE_ALL, "First Child", firstChild);
	RTTI_DEFINE_MEMBER(Node, SERIALIZE_ALL, "Prev Sibling", prevSibling);
	RTTI_DEFINE_MEMBER(Node, SERIALIZE_ALL, "Next Sibling", nextSibling);
}


RTTI_DEFINE_TYPE(Mesh)
{
	RTTI_DEFINE_MEMBER(Mesh, SERIALIZE_ALL, "Positions", positions);
	RTTI_DEFINE_MEMBER(Mesh, SERIALIZE_ALL, "Texcoords", uvs);
	RTTI_DEFINE_MEMBER(Mesh, SERIALIZE_ALL, "Normals", normals);
	RTTI_DEFINE_MEMBER(Mesh, SERIALIZE_ALL, "Tangents", tangents);
	RTTI_DEFINE_MEMBER(Mesh, SERIALIZE_ALL, "Indices", indices);
	RTTI_DEFINE_MEMBER(Mesh, SERIALIZE_ALL, "Material", material);
}

RTTI_DEFINE_TYPE(BoxCollider) {}
RTTI_DEFINE_TYPE(SoftBody) {}


RTTI_DEFINE_TYPE(Bone)
{
	RTTI_DEFINE_MEMBER(Bone, SERIALIZE_ALL, "Index", index);
	RTTI_DEFINE_MEMBER(Bone, SERIALIZE_ALL, "Name", name);
	RTTI_DEFINE_MEMBER(Bone, SERIALIZE_ALL, "Children", children);
}


RTTI_DEFINE_TYPE(Skeleton)
{
	RTTI_DEFINE_MEMBER(Skeleton, SERIALIZE_ALL, "Inv Global Transform", inverseGlobalTransform);
	RTTI_DEFINE_MEMBER(Skeleton, SERIALIZE_ALL, "Bone Weights", boneWeights);
	RTTI_DEFINE_MEMBER(Skeleton, SERIALIZE_ALL, "Bone Indices", boneIndices);
	RTTI_DEFINE_MEMBER(Skeleton, SERIALIZE_ALL, "Bone Offset Matrices", boneOffsetMatrices);
	RTTI_DEFINE_MEMBER(Skeleton, SERIALIZE_ALL, "Bone Transform Matrices", boneTransformMatrices);
	RTTI_DEFINE_MEMBER(Skeleton, SERIALIZE_ALL, "Bone Hierarchy", boneHierarchy);
	RTTI_DEFINE_MEMBER(Skeleton, SERIALIZE_ALL, "Animations", animations);
}


RTTI_DEFINE_TYPE(NativeScript) {}


RTTI_DEFINE_TYPE(Material)
{
	RTTI_DEFINE_MEMBER(Material, SERIALIZE_ALL, "Base Color", albedo);
	RTTI_DEFINE_MEMBER(Material, SERIALIZE_ALL, "Base Emissive", emissive);
	RTTI_DEFINE_MEMBER(Material, SERIALIZE_ALL, "Metallic", metallic);
	RTTI_DEFINE_MEMBER(Material, SERIALIZE_ALL, "Roughness", roughness);
	RTTI_DEFINE_MEMBER(Material, SERIALIZE_ALL, "Transparency", isTransparent);
	RTTI_DEFINE_MEMBER(Material, SERIALIZE_ALL, "Albedo Map", albedoFile);
	RTTI_DEFINE_MEMBER(Material, SERIALIZE_ALL, "Normal Map", normalFile);
	RTTI_DEFINE_MEMBER(Material, SERIALIZE_ALL, "Emissive Map", emissiveFile);
	RTTI_DEFINE_MEMBER(Material, SERIALIZE_ALL, "Metallic Map", metallicFile);
	RTTI_DEFINE_MEMBER(Material, SERIALIZE_ALL, "Roughness Map", roughnessFile);
	RTTI_DEFINE_MEMBER(Material, SERIALIZE_ALL, "Albedo Map Swizzle", gpuAlbedoMapSwizzle);
	RTTI_DEFINE_MEMBER(Material, SERIALIZE_ALL, "Normal Map Swizzle", gpuNormalMapSwizzle);
	RTTI_DEFINE_MEMBER(Material, SERIALIZE_ALL, "Emissive Map Swizzle", gpuEmissiveMapSwizzle);
	RTTI_DEFINE_MEMBER(Material, SERIALIZE_ALL, "Metallic Map Swizzle", gpuMetallicMapSwizzle);
	RTTI_DEFINE_MEMBER(Material, SERIALIZE_ALL, "Roughness Map Swizzle", gpuRoughnessMapSwizzle);
}


void gRegisterComponentTypes()
{
	g_RTTIFactory.Register(RTTI_OF(Name));
	g_RTTIFactory.Register(RTTI_OF(Transform));
	g_RTTIFactory.Register(RTTI_OF(Mesh));
	g_RTTIFactory.Register(RTTI_OF(Node));
	g_RTTIFactory.Register(RTTI_OF(Material));
	g_RTTIFactory.Register(RTTI_OF(BoxCollider));
	g_RTTIFactory.Register(RTTI_OF(DirectionalLight));
	g_RTTIFactory.Register(RTTI_OF(PointLight));
	g_RTTIFactory.Register(RTTI_OF(SoftBody));
	g_RTTIFactory.Register(RTTI_OF(Bone));
	g_RTTIFactory.Register(RTTI_OF(Skeleton));
	g_RTTIFactory.Register(RTTI_OF(NativeScript));
}


Material Material::Default;


void Transform::Compose()
{
	localTransform = glm::translate(glm::mat4(1.0f), position);
	localTransform = localTransform * glm::toMat4(rotation);
	localTransform = glm::scale(localTransform, scale);
}


void Transform::Decompose()
{
	position = localTransform[3];

	for (int i = 0; i < 3; i++)
		scale[i] = glm::length(glm::vec3(localTransform[i]));

	const auto rotation_matrix = glm::mat3
	(
		glm::vec3(localTransform[0]) / scale[0],
		glm::vec3(localTransform[1]) / scale[1],
		glm::vec3(localTransform[2]) / scale[2]
	);

	rotation = glm::quat_cast(rotation_matrix);
}


Vec3 Transform::GetScaleWorldSpace() const
{
	auto result = Vec3(1.0f);

	for (int i = 0; i < 3; i++)
		result[i] = glm::length(glm::vec3(worldTransform[i]));

	return result;
}


Vec3 Transform::GetPositionWorldSpace() const
{
	return worldTransform[3];
}


Quat Transform::GetRotationWorldSpace() const
{
	const auto rotation_matrix = glm::mat3
	(
		glm::vec3(worldTransform[0]) / scale[0],
		glm::vec3(worldTransform[1]) / scale[1],
		glm::vec3(worldTransform[2]) / scale[2]
	);

	return glm::quat_cast(rotation_matrix);
}


void Transform::Print()
{
	std::cout << glm::to_string(worldTransform) << '\n';
}


void Mesh::CalculateTangents(float inTangentSign)
{
	tangents.resize(positions.size());

	//// calculate the tangent and bitangent for every face
	for (size_t i = 0; i < indices.size(); i += 3)
	{
		auto p0 = indices[i];
		auto p1 = indices[i + 1];
		auto p2 = indices[i + 2];

		// position differences p1->p2 and p1->p3
		Vec3 v = positions[p1] - positions[p0], w = positions[p2] - positions[p0];

		// texture offset p1->p2 and p1->p3
		float sx = uvs[p1].x - uvs[p0].x, sy = uvs[p1].y - uvs[p0].y;
		float tx = uvs[p2].x - uvs[p0].x, ty = uvs[p2].y - uvs[p0].y;
		float dirCorrection = ( tx * sy - ty * sx ) < 0.0f ? -1.0f : 1.0f;
		// when t1, t2, t3 in same position in UV space, just use default UV direction.
		if (sx * ty == sy * tx)
		{
			sx = 0.0;
			sy = 1.0;
			tx = 1.0;
			ty = 0.0;
		}

		// tangent points in the direction where to positive X axis of the texture coord's would point in model space
		// bitangent's points along the positive Y axis of the texture coord's, respectively
		Vec3 tangent, bitangent;
		tangent.x = ( w.x * sy - v.x * ty ) * dirCorrection;
		tangent.y = ( w.y * sy - v.y * ty ) * dirCorrection;
		tangent.z = ( w.z * sy - v.z * ty ) * dirCorrection;
		bitangent.x = ( -w.x * sx + v.x * tx ) * dirCorrection;
		bitangent.y = ( -w.y * sx + v.y * tx ) * dirCorrection;
		bitangent.z = ( -w.z * sx + v.z * tx ) * dirCorrection;

		// store for every vertex of that face
		for (unsigned int b = 0; b < 3; ++b)
		{
			auto p = indices[i + b];

			// project tangent and bitangent into the plane formed by the vertex' normal
			glm::vec3 localTangent = tangent - normals[p] * ( tangent * normals[p] );
			glm::vec3 localBitangent = bitangent - normals[p] * ( bitangent * normals[p] ) - localTangent * ( bitangent * localTangent );
			localTangent = glm::normalize(localTangent);
			localBitangent = glm::normalize(localBitangent);

			auto is_special_float = [](float inValue) { return std::isnan(inValue) || std::isinf(inValue); };

			// reconstruct tangent/bitangent according to normal and bitangent/tangent when it's infinite or NaN.
			bool invalid_tangent = is_special_float(localTangent.x) || is_special_float(localTangent.y) || is_special_float(localTangent.z);
			bool invalid_bitangent = is_special_float(localBitangent.x) || is_special_float(localBitangent.y) || is_special_float(localBitangent.z);
			if (invalid_tangent != invalid_bitangent)
			{
				if (invalid_tangent)
				{
					localTangent = glm::normalize(localTangent);
				}
				else
				{
					localBitangent = glm::normalize(localBitangent);
				}
			}

			// and write it into the mesh.
			tangents[p] = localTangent;

			if (glm::dot(glm::cross(normals[p], tangents[p]), localBitangent) * inTangentSign < 0.0f)
			{
				tangents[p] = tangents[p] * -1.0f;
			}
		}
	}
}


void Mesh::CalculateNormals()
{
	normals.resize(positions.size(), glm::vec3(0.0f));

	for (auto i = 0; i < indices.size(); i += 3)
	{
		auto normal = glm::normalize(glm::cross(
			positions[indices[i]] - positions[indices[i + 1]],
			positions[indices[i + 1]] - positions[indices[i + 2]]
		));

		normals[indices[i]] += normal;
		normals[indices[i + 1]] += normal;
		normals[indices[i + 2]] += normal;
	}

	for (auto& normal : normals)
		normal = glm::normalize(normal / 3.0f);
}


void Mesh::CalculateAABB()
{
	if (positions.size() < 2)
		return;

	aabb[0] = positions[0];
	aabb[1] = positions[1];

	for (const auto& v : positions)
	{
		aabb[0] = glm::min(aabb[0], v);
		aabb[1] = glm::max(aabb[1], v);
	}
}


const std::vector<float>& Mesh::GetInterleavedVertices()
{
	mInterleavedVertices.clear();

	mInterleavedVertices.reserve(
		3 * positions.size() +
		2 * uvs.size() +
		3 * normals.size() +
		3 * tangents.size()
	);

	const bool hasUVs = !uvs.empty();
	const bool hasNormals = !normals.empty();
	const bool hasTangents = !tangents.empty();

	for (auto i = 0; i < positions.size(); i++)
	{
		auto position = positions[i];
		mInterleavedVertices.push_back(position.x);
		mInterleavedVertices.push_back(position.y);
		mInterleavedVertices.push_back(position.z);

		if (hasUVs)
		{
			auto uv = uvs[i];
			mInterleavedVertices.push_back(uv.x);
			mInterleavedVertices.push_back(uv.y);
		}

		if (hasNormals)
		{
			auto normal = normals[i];
			mInterleavedVertices.push_back(normal.x);
			mInterleavedVertices.push_back(normal.y);
			mInterleavedVertices.push_back(normal.z);
		}

		if (hasTangents && i < tangents.size())
		{
			auto tangent = tangents[i];
			mInterleavedVertices.push_back(tangent.x);
			mInterleavedVertices.push_back(tangent.y);
			mInterleavedVertices.push_back(tangent.z);
		}
	}

	return mInterleavedVertices;
}



uint32_t Mesh::GetInterleavedStride() const
{
	auto stride = 0u;
	stride += sizeof(glm::vec3) * !positions.empty();
	stride += sizeof(glm::vec2) * !uvs.empty();
	stride += sizeof(glm::vec3) * !normals.empty();
	stride += sizeof(glm::vec3) * !tangents.empty();
	return stride;
}



void Skeleton::UpdateBoneTransforms(const Animation& animation, float animationTime, Bone& pNode, const glm::mat4& parentTransform)
{
	auto global_transform = glm::mat4(1.0f);

	bool has_animation = animation.m_BoneAnimations.find(pNode.index) != animation.m_BoneAnimations.end();

	if (pNode.index != boneHierarchy.index && has_animation)
	{
		const auto& node_anim = animation.m_BoneAnimations.at(pNode.index);

		auto translation = node_anim.GetInterpolatedPosition(animationTime);
		auto translation_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(translation.x, translation.y, translation.z));

		auto rotation = node_anim.GetInterpolatedRotation(animationTime);
		auto rotation_matrix = glm::toMat4(rotation);

		auto scale = node_anim.GetInterpolatedScale(animationTime);
		auto scale_matrix = glm::scale(glm::mat4(1.0f), glm::vec3(scale.x, scale.y, scale.z));

		auto nodeTransform = translation_matrix * rotation_matrix * scale_matrix;

		global_transform = parentTransform * nodeTransform;

		boneTransformMatrices[pNode.index] = global_transform * boneOffsetMatrices[pNode.index];
	}

	for (auto& child : pNode.children)
		UpdateBoneTransforms(animation, animationTime, child, global_transform);
}


void Skeleton::UpdateFromAnimation(Animation& animation, float dt)
{
	/*
		This is bugged, Assimp docs say totalDuration is in ticks, but the actual value is real world time in milliseconds
		see https://github.com/assimp/assimp/issues/2662
	*/
	animation.m_RunningTime += Timer::sToMilliseconds(dt);

	if (animation.m_RunningTime > animation.m_TotalDuration)
		animation.m_RunningTime = 0;

	auto identity = glm::mat4(1.0f);
	UpdateBoneTransforms(animation, animation.m_RunningTime, boneHierarchy, identity);
}



template<>
void clone<Transform>(ecs::ECS& reg, Entity from, Entity to)
{
	auto& component = reg.Get<Transform>(from);
	reg.Add<Transform>(to, component);
}


template<>
void clone<Node>(ecs::ECS& reg, Entity from, Entity to)
{
	auto& fromNode = reg.Get<Node>(from);
	auto& toNode = reg.Add<Node>(to);
	/*if (fromNode.parent != NULL_ENTITY) {
		NodeSystem::sAppend(reg, fromNode.parent, reg.Get<Node>(fromNode.parent), to, toNode);
	} TODO: FIXME: pls fix */
}


template<>
void clone<Name>(ecs::ECS& reg, Entity from, Entity to)
{
	auto& component = reg.Get<Name>(from);
	reg.Add<Name>(to, component);
}


template<>
void clone<Mesh>(ecs::ECS& reg, Entity from, Entity to)
{
	auto& from_component = reg.Get<Mesh>(from);
	auto& to_component = reg.Add<Mesh>(to, from_component);
}


template<>
void clone<Material>(ecs::ECS& reg, Entity from, Entity to)
{
	auto& from_component = reg.Get<Material>(from);
	auto& to_component = reg.Add<Material>(to, from_component);
}


template<>
void clone<BoxCollider>(ecs::ECS& reg, Entity from, Entity to)
{
	auto& from_component = reg.Get<BoxCollider>(from);
	auto& to_component = reg.Add<BoxCollider>(to, from_component);

	// Invalidate the copied bodyID so it gets registered next update
	to_component.bodyID = JPH::BodyID();
}

void SoftBody::CreateFromMesh(const Mesh& inMesh)
{

}

} // raekor