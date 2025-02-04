#include "PCH.h"
#include "Components.h"
#include "Timer.h"
#include "Scene.h"
#include "Assets.h"
#include "Member.h"
#include "Physics.h"
#include "Primitives.h"
#include "DebugRenderer.h"

namespace RK {

RTTI_DEFINE_TYPE(SceneComponent)
{
	RTTI_DEFINE_TYPE_INHERITANCE(SceneComponent, Component);
}

RTTI_DEFINE_TYPE(Name)
{
	RTTI_DEFINE_TYPE_INHERITANCE(Name, Component);
	RTTI_DEFINE_MEMBER(Name, SERIALIZE_ALL, "Name", name);
}


RTTI_DEFINE_TYPE(Transform)
{
	RTTI_DEFINE_TYPE_INHERITANCE(Transform, Component);
	RTTI_DEFINE_MEMBER(Transform, SERIALIZE_ALL, "Scale", scale);
	RTTI_DEFINE_MEMBER(Transform, SERIALIZE_ALL, "Position", position);
	RTTI_DEFINE_MEMBER(Transform, SERIALIZE_ALL, "Rotation", rotation);
	RTTI_DEFINE_MEMBER(Transform, SERIALIZE_ALL, "Local Transform", localTransform);
	RTTI_DEFINE_MEMBER(Transform, SERIALIZE_ALL, "World Transform", worldTransform);
}


RTTI_DEFINE_TYPE(DirectionalLight)
{
	RTTI_DEFINE_TYPE_INHERITANCE(DirectionalLight, SceneComponent);
	RTTI_DEFINE_MEMBER(DirectionalLight, SERIALIZE_ALL, "Direction", direction);
	RTTI_DEFINE_MEMBER(DirectionalLight, SERIALIZE_ALL, "Color", color);
	RTTI_DEFINE_MEMBER(DirectionalLight, SERIALIZE_ALL, "Skybox", cubeMapFile);
}


RTTI_DEFINE_ENUM(ELightType)
{
	RTTI_DEFINE_ENUM_MEMBER(ELightType, SERIALIZE_ALL, "None", LIGHT_TYPE_NONE);
	RTTI_DEFINE_ENUM_MEMBER(ELightType, SERIALIZE_ALL, "Spot", LIGHT_TYPE_SPOT);
	RTTI_DEFINE_ENUM_MEMBER(ELightType, SERIALIZE_ALL, "Point", LIGHT_TYPE_POINT);
}


RTTI_DEFINE_TYPE(Light)
{
	RTTI_DEFINE_TYPE_INHERITANCE(Light, SceneComponent);
	RTTI_DEFINE_MEMBER(Light, SERIALIZE_ALL, "Type", type);
	RTTI_DEFINE_MEMBER(Light, SERIALIZE_ALL, "Direction", direction);
	RTTI_DEFINE_MEMBER(Light, SERIALIZE_ALL, "Position", position);
	RTTI_DEFINE_MEMBER(Light, SERIALIZE_ALL, "Color", color);
	RTTI_DEFINE_MEMBER(Light, SERIALIZE_ALL, "Attributes", attributes);
}


RTTI_DEFINE_TYPE(Mesh)
{
	RTTI_DEFINE_TYPE_INHERITANCE(Mesh, Component);
	RTTI_DEFINE_MEMBER(Mesh, SERIALIZE_ALL, "BBox", bbox);
	RTTI_DEFINE_MEMBER(Mesh, SERIALIZE_ALL, "Positions", positions);
	RTTI_DEFINE_MEMBER(Mesh, SERIALIZE_ALL, "Texcoords", uvs);
	RTTI_DEFINE_MEMBER(Mesh, SERIALIZE_ALL, "Normals", normals);
	RTTI_DEFINE_MEMBER(Mesh, SERIALIZE_ALL, "Tangents", tangents);
	RTTI_DEFINE_MEMBER(Mesh, SERIALIZE_ALL, "Vertices", vertices);
	RTTI_DEFINE_MEMBER(Mesh, SERIALIZE_ALL, "Indices", indices);
	RTTI_DEFINE_MEMBER(Mesh, SERIALIZE_ALL, "Material", material);
}


RTTI_DEFINE_TYPE(RigidBody) 
{
	RTTI_DEFINE_TYPE_INHERITANCE(RigidBody, SceneComponent);
}


RTTI_DEFINE_TYPE(SoftBody) 
{
	RTTI_DEFINE_TYPE_INHERITANCE(SoftBody, SceneComponent);
}


RTTI_DEFINE_TYPE(Skeleton::Bone)
{
	RTTI_DEFINE_TYPE_INHERITANCE(Skeleton::Bone, Component);
	RTTI_DEFINE_MEMBER(Skeleton::Bone, SERIALIZE_ALL, "Index", index);
	RTTI_DEFINE_MEMBER(Skeleton::Bone, SERIALIZE_ALL, "Name", name);
	RTTI_DEFINE_MEMBER(Skeleton::Bone, SERIALIZE_ALL, "Children", children);
}


RTTI_DEFINE_TYPE(Skeleton)
{
	RTTI_DEFINE_TYPE_INHERITANCE(Skeleton, Component);
	RTTI_DEFINE_MEMBER(Skeleton, SERIALIZE_ALL, "Animation", animation);
	RTTI_DEFINE_MEMBER(Skeleton, SERIALIZE_ALL, "Inv Global Transform", inverseGlobalTransform);
	RTTI_DEFINE_MEMBER(Skeleton, SERIALIZE_ALL, "Bone Weights", boneWeights);
	RTTI_DEFINE_MEMBER(Skeleton, SERIALIZE_ALL, "Bone Indices", boneIndices);
	RTTI_DEFINE_MEMBER(Skeleton, SERIALIZE_ALL, "Bone Offset Matrices", boneOffsetMatrices);
	RTTI_DEFINE_MEMBER(Skeleton, SERIALIZE_ALL, "Bone Hierarchy", rootBone);
}


RTTI_DEFINE_TYPE(NativeScript) 
{
	RTTI_DEFINE_TYPE_INHERITANCE(NativeScript, SceneComponent);
	RTTI_DEFINE_MEMBER(NativeScript, SERIALIZE_ALL, "File", file);
	RTTI_DEFINE_MEMBER(NativeScript, SERIALIZE_ALL, "Type", type);
}


RTTI_DEFINE_TYPE(Material)
{
	RTTI_DEFINE_TYPE_INHERITANCE(Material, Component);
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


RTTI_DEFINE_TYPE(AudioStream)
{

}


RTTI_DEFINE_TYPE(DDGISceneSettings) 
{
	RTTI_DEFINE_TYPE_INHERITANCE(DDGISceneSettings, Component);
	RTTI_DEFINE_MEMBER(DDGISceneSettings, SERIALIZE_ALL, "Debug Probe", mDDGIDebugProbe);
	RTTI_DEFINE_MEMBER(DDGISceneSettings, SERIALIZE_ALL, "Probe Count", mDDGIProbeCount);
	RTTI_DEFINE_MEMBER(DDGISceneSettings, SERIALIZE_ALL, "Probe Spacing", mDDGIProbeSpacing);
}


void gRegisterComponentTypes()
{
	g_RTTIFactory.Register(RTTI_OF<Entity>());
	g_RTTIFactory.Register(RTTI_OF<Name>());
	g_RTTIFactory.Register(RTTI_OF<Transform>());
    g_RTTIFactory.Register(RTTI_OF<Mesh>());
    g_RTTIFactory.Register(RTTI_OF<Camera>());
	g_RTTIFactory.Register(RTTI_OF<Material>());
	g_RTTIFactory.Register(RTTI_OF<Animation>());
	g_RTTIFactory.Register(RTTI_OF<RigidBody>());
	g_RTTIFactory.Register(RTTI_OF<DirectionalLight>());
	g_RTTIFactory.Register(RTTI_OF<Light>());
	g_RTTIFactory.Register(RTTI_OF<SoftBody>());
	g_RTTIFactory.Register(RTTI_OF<Skeleton>());
	g_RTTIFactory.Register(RTTI_OF<Skeleton::Bone>());
	g_RTTIFactory.Register(RTTI_OF<NativeScript>());
	g_RTTIFactory.Register(RTTI_OF<DDGISceneSettings>());
}


Material Material::Default;


void Transform::Scale(Vec3 inScale)
{
    scale += inScale;
}


void Transform::Translate(Vec3 inTranslation)
{
    position += inTranslation;
}


void Transform::Rotate(Quat inRotation)
{
    rotation *= inRotation;
}


void Transform::Rotate(float inDegrees, Vec3 inAxis)
{
    Rotate(glm::angleAxis(glm::radians(inDegrees), inAxis));
}


void Transform::Compose()
{
	localTransform = glm::translate(Mat4x4(1.0f), position);
	localTransform = localTransform * glm::toMat4(rotation);
	localTransform = glm::scale(localTransform, scale);
}


void Transform::Decompose()
{
	position = localTransform[3];

	for (int i = 0; i < 3; i++)
		scale[i] = glm::length(Vec3(localTransform[i]));

	const Mat3x3 rotation_matrix = Mat3x3
	(
		Vec3(localTransform[0]) / scale[0],
		Vec3(localTransform[1]) / scale[1],
		Vec3(localTransform[2]) / scale[2]
	);

	rotation = glm::quat_cast(rotation_matrix);
}


Vec3 Transform::GetScaleWorldSpace() const
{
	Vec3 result = Vec3(1.0f);

	for (int i = 0; i < 3; i++)
		result[i] = glm::length(Vec3(worldTransform[i]));

	return result;
}


Vec3 Transform::GetPositionWorldSpace() const
{
	return worldTransform[3];
}


Quat Transform::GetRotationWorldSpace() const
{
	const Mat3x3 rotation_matrix = Mat3x3
	(
		Vec3(worldTransform[0]) / scale[0],
		Vec3(worldTransform[1]) / scale[1],
		Vec3(worldTransform[2]) / scale[2]
	);

	return glm::quat_cast(rotation_matrix);
}


void Transform::Print()
{
	std::cout << glm::to_string(worldTransform) << '\n';
}


void Mesh::CreateCube(Mesh& ioMesh, float inScale)
{
	static constexpr std::array vertices = {
		// Front face
		Vertex(Vec3(-0.5f, -0.5f,  0.5f), Vec2(0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)),
		Vertex(Vec3(0.5f, -0.5f,  0.5f),  Vec2(1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)),
		Vertex(Vec3(0.5f,  0.5f,  0.5f),  Vec2(1.0f, 1.0f), Vec3(0.0f, 0.0f, 1.0f)),
		Vertex(Vec3(-0.5f,  0.5f,  0.5f), Vec2(0.0f, 1.0f), Vec3(0.0f, 0.0f, 1.0f)),

		// Back face
		Vertex(Vec3(-0.5f, -0.5f, -0.5f), Vec2(1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)),
		Vertex(Vec3(-0.5f,  0.5f, -0.5f), Vec2(1.0f, 1.0f), Vec3(0.0f, 0.0f, -1.0f)),
		Vertex(Vec3(0.5f,  0.5f, -0.5f),  Vec2(0.0f, 1.0f), Vec3(0.0f, 0.0f, -1.0f)),
		Vertex(Vec3(0.5f, -0.5f, -0.5f),  Vec2(0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)),

		// Left face
		Vertex(Vec3(-0.5f, -0.5f, -0.5f), Vec2(0.0f, 0.0f), Vec3(-1.0f, 0.0f, 0.0f)),
		Vertex(Vec3(-0.5f, -0.5f,  0.5f), Vec2(1.0f, 0.0f), Vec3(-1.0f, 0.0f, 0.0f)),
		Vertex(Vec3(-0.5f,  0.5f,  0.5f), Vec2(1.0f, 1.0f), Vec3(-1.0f, 0.0f, 0.0f)),
		Vertex(Vec3(-0.5f,  0.5f, -0.5f), Vec2(0.0f, 1.0f), Vec3(-1.0f, 0.0f, 0.0f)),

		// Right face
		Vertex(Vec3(0.5f, -0.5f, -0.5f), Vec2(0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f)),
		Vertex(Vec3(0.5f,  0.5f, -0.5f), Vec2(0.0f, 1.0f), Vec3(1.0f, 0.0f, 0.0f)),
		Vertex(Vec3(0.5f,  0.5f,  0.5f), Vec2(1.0f, 1.0f), Vec3(1.0f, 0.0f, 0.0f)),
		Vertex(Vec3(0.5f, -0.5f,  0.5f), Vec2(1.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f)),

		// Top face
		Vertex(Vec3(-0.5f,  0.5f,  0.5f), Vec2(0.0f, 1.0f), Vec3(0.0f, 1.0f, 0.0f)),
		Vertex(Vec3(0.5f,  0.5f,  0.5f),  Vec2(1.0f, 1.0f), Vec3(0.0f, 1.0f, 0.0f)),
		Vertex(Vec3(0.5f,  0.5f, -0.5f),  Vec2(1.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f)),
		Vertex(Vec3(-0.5f,  0.5f, -0.5f), Vec2(0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f)),

		// Bottom face
		Vertex(Vec3(-0.5f, -0.5f,  0.5f), Vec2(0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
		Vertex(Vec3(-0.5f, -0.5f, -0.5f), Vec2(0.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f)),
		Vertex(Vec3(0.5f, -0.5f, -0.5f),  Vec2(1.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f)),
		Vertex(Vec3(0.5f, -0.5f,  0.5f),  Vec2(1.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
	};

	static constexpr std::array indices = {
		0, 1, 2, 2, 3, 0,       // Front  face
		4, 5, 6, 6, 7, 4,       // Back   face
		8, 9, 10, 10, 11, 8,    // Left   face
		12, 13, 14, 14, 15, 12, // Right  face
		16, 17, 18, 18, 19, 16, // Top    face
		20, 21, 22, 22, 23, 20  // Bottom face
	};

	for (const Vertex& v : vertices)
	{
		ioMesh.uvs.push_back(v.uv);
		ioMesh.normals.push_back(v.normal);
		ioMesh.positions.push_back(v.pos * inScale);
	}

	for (uint32_t index : indices)
		ioMesh.indices.push_back(index);

	ioMesh.CalculateTangents();
	ioMesh.CalculateVertices();
	ioMesh.CalculateBoundingBox();
}


void Mesh::CreatePlane(Mesh& ioMesh, float inScale)
{
	static constexpr std::array vertices = 
	{
		Vertex{{-0.5f, 0.0f, -0.5f}, {0.0f, 0.0f}, {0.0, 1.0, 0.0}, {}},
		Vertex{{ 0.5f, 0.0f, -0.5f}, {1.0f, 0.0f}, {0.0, 1.0, 0.0}, {}},
		Vertex{{ 0.5f, 0.0f,  0.5f}, {1.0f, 1.0f}, {0.0, 1.0, 0.0}, {}},
		Vertex{{-0.5f, 0.0f,  0.5f}, {0.0f, 1.0f}, {0.0, 1.0, 0.0}, {}}
	};

	static constexpr std::array indices = 
	{
		Triangle{3, 1, 0}, Triangle{2, 1, 3}
	};

	for (const Vertex& v : vertices)
	{
		ioMesh.uvs.push_back(v.uv);
		ioMesh.normals.push_back(v.normal);
		ioMesh.positions.push_back(v.pos * inScale);
	}

	for (const Triangle& triangle : indices)
	{
		ioMesh.indices.push_back(triangle.p1);
		ioMesh.indices.push_back(triangle.p2);
		ioMesh.indices.push_back(triangle.p3);
	}

	ioMesh.CalculateTangents();
	ioMesh.CalculateVertices();
	ioMesh.CalculateBoundingBox();
}


void Mesh::CreateSphere(Mesh& ioMesh, float inRadius, uint32_t inSectorCount, uint32_t inStackCount)
{
	ioMesh.positions.clear();
	ioMesh.normals.clear();
	ioMesh.tangents.clear();
	ioMesh.uvs.clear();

	float x, y, z, xy;                              // vertex position
	float nx, ny, nz, lengthInv = 1.0f / inRadius;  // vertex normal
	float s, t;                                     // vertex texCoord

	const float PI = static_cast<float>( M_PI );
	float sectorStep = 2 * PI / inSectorCount;
	float stackStep = PI / inStackCount;
	float sectorAngle, stackAngle;

	for (uint32_t i = 0; i <= inStackCount; ++i)
	{
		stackAngle = PI / 2 - i * stackStep;        // starting from pi/2 to -pi/2
		xy = inRadius * cosf(stackAngle);             // r * cos(u)
		z = inRadius * sinf(stackAngle);              // r * sin(u)

		// add (sectorCount+1) vertices per stack
		// the first and last vertices have same position and normal, but different tex coords
		for (uint32_t j = 0; j <= inSectorCount; ++j)
		{
			sectorAngle = j * sectorStep;           // starting from 0 to 2pi

			// vertex position (x, y, z)
			x = xy * cosf(sectorAngle);             // r * cos(u) * cos(v)
			y = xy * sinf(sectorAngle);             // r * cos(u) * sin(v)
			ioMesh.positions.emplace_back(x, y, z);

			// normalized vertex normal (nx, ny, nz)
			nx = x * lengthInv;
			ny = y * lengthInv;
			nz = z * lengthInv;
			ioMesh.normals.emplace_back(nx, ny, nz);

			// vertex tex coord (s, t) range between [0, 1]
			s = (float)j / inSectorCount;
			t = (float)i / inStackCount;
			ioMesh.uvs.emplace_back(s, t);

		}
	}

	int k1, k2;
	for (uint32_t i = 0; i < inStackCount; ++i)
	{
		k1 = i * ( inSectorCount + 1 );     // beginning of current stack
		k2 = k1 + inSectorCount + 1;      // beginning of next stack

		for (uint32_t j = 0; j < inSectorCount; ++j, ++k1, ++k2)
		{
			// 2 triangles per sector excluding first and last stacks
			// k1 => k2 => k1+1
			if (i != 0)
			{
				ioMesh.indices.push_back(k1);
				ioMesh.indices.push_back(k2);
				ioMesh.indices.push_back(k1 + 1);
			}

			// k1+1 => k2 => k2+1
			if (i != ( inStackCount - 1 ))
			{
				ioMesh.indices.push_back(k1 + 1);
				ioMesh.indices.push_back(k2);
				ioMesh.indices.push_back(k2 + 1);
			}
		}
	}
	
	ioMesh.CalculateNormals();
	ioMesh.CalculateTangents();
	ioMesh.CalculateVertices();
	ioMesh.CalculateBoundingBox();
}


void Mesh::CalculateTangents()
{
	tangents.resize(positions.size());

	//// calculate the tangent and bitangent for every face
	for (size_t i = 0; i < indices.size(); i += 3)
	{
		uint32_t p0 = indices[glm::min(i + 0, indices.size() - 1)];
		uint32_t p1 = indices[glm::min(i + 1, indices.size() - 1)];
		uint32_t p2 = indices[glm::min(i + 2, indices.size() - 1)];

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
			uint32_t p = indices[glm::min(i + b, indices.size() - 1)];

			// project tangent and bitangent into the plane formed by the vertex' normal
			Vec3 localTangent = tangent - normals[p] * ( tangent * normals[p] );
			Vec3 localBitangent = bitangent - normals[p] * ( bitangent * normals[p] ) - localTangent * ( bitangent * localTangent );
			localTangent = glm::normalize(localTangent);
			localBitangent = glm::normalize(localBitangent);

			auto is_special_float = [](float inValue) { return std::isnan(inValue) || std::isinf(inValue); };

			// reconstruct tangent/bitangent according to normal and bitangent/tangent when it's infinite or NaN.
			bool invalid_tangent = is_special_float(localTangent.x) || is_special_float(localTangent.y) || is_special_float(localTangent.z);
			bool invalid_bitangent = is_special_float(localBitangent.x) || is_special_float(localBitangent.y) || is_special_float(localBitangent.z);
			if (invalid_tangent != invalid_bitangent)
			{
				if (invalid_tangent)
					localTangent = glm::normalize(localTangent);
				else
					localBitangent = glm::normalize(localBitangent);
			}

			// and write it into the mesh.
			tangents[p] = localTangent;

			if (glm::dot(glm::cross(normals[p], tangents[p]), localBitangent) < 0.0f)
			{
				tangents[p] = tangents[p] * -1.0f;
			}
		}
	}
}


void Mesh::CalculateNormals()
{
	normals.resize(positions.size(), Vec3(0.0f));

	for (int i = 0; i < indices.size(); i += 3)
	{
		const uint32_t i0 = indices[i];
		const uint32_t i1 = indices[i + 1];
		const uint32_t i2 = indices[i + 2];

		const Vec3 p0 = positions[i0];
		const Vec3 p1 = positions[i1];
		const Vec3 p2 = positions[i2];

		const Vec3 v0 = glm::normalize(p0 - p1);
		const Vec3 v1 = glm::normalize(p1 - p2);

		Vec3 normal = glm::cross(v0, v1);

		if (glm::length(normal) > 0.0f)
			normal = glm::normalize(normal);

		normals[indices[i]] = normal;
		normals[indices[i + 1]] = normal;
		normals[indices[i + 2]] = normal;

		for (int i = 0; i < 3; i++)
			assert(!glm::isnan(normal[i]));
	}
}


void Mesh::CalculateBoundingBox()
{
	if (positions.size() < 2)
		return;

	bbox.mMin = positions[0];
	bbox.mMax = positions[1];

	for (const Vec3& pos : positions)
	{
		bbox.mMin = glm::min(bbox.mMin, pos);
		bbox.mMax = glm::max(bbox.mMax, pos);
	}
}


void Mesh::CalculateVertices()
{
	vertices.clear();

	vertices.reserve(
		3 * positions.size() +
		2 * uvs.size() +
		3 * normals.size() +
		3 * tangents.size()
	);

	const bool has_uvs = !uvs.empty();
	const bool has_normals = !normals.empty();
	const bool has_tangents = !tangents.empty();

	for (int i = 0; i < positions.size(); i++)
	{
		Vec3 position = positions[i];
		vertices.push_back(position.x);
		vertices.push_back(position.y);
		vertices.push_back(position.z);

		if (has_uvs)
		{
			Vec2 uv = uvs[i];
			vertices.push_back(uv.x);
			vertices.push_back(uv.y);
		}

		if (has_normals)
		{
			Vec3 normal = normals[i];
			vertices.push_back(normal.x);
			vertices.push_back(normal.y);
			vertices.push_back(normal.z);
		}

		if (has_tangents && i < tangents.size())
		{
			Vec3 tangent = tangents[i];
			vertices.push_back(tangent.x);
			vertices.push_back(tangent.y);
			vertices.push_back(tangent.z);
		}
	}
}


uint32_t Mesh::GetVertexStride() const
{
	uint32_t stride = 0u;
	stride += sizeof(Vec3) * !positions.empty();
	stride += sizeof(Vec2) * !uvs.empty();
	stride += sizeof(Vec3) * !normals.empty();
	stride += sizeof(Vec3) * !tangents.empty();
	return stride;
}


void Skeleton::DebugDraw(const Bone& inBone, const Mat4x4& inTransform)
{
	const Vec3& parent_pos = (inTransform * boneWSTransformMatrices[inBone.index])[3];

	for (const Bone& bone : inBone.children)
	{
		const Vec3& child_pos = (inTransform * boneWSTransformMatrices[bone.index])[3];

		g_DebugRenderer.AddLine(parent_pos, child_pos);

		DebugDraw(bone, inTransform);
	}
}



void Skeleton::UpdateBoneTransform(const Animation& animation, Bone& inBone, const Mat4x4& parentTransform)
{
	Mat4x4 global_transform = Mat4x4(1.0f);

	bool has_animation = animation.m_KeyFrames.contains(inBone.name);

	if (inBone.index != rootBone.index && has_animation)
	{
		const KeyFrames& keyframes = animation.m_KeyFrames.at(inBone.name);

		Vec3 translation = keyframes.GetInterpolatedPosition(animation.GetRunningTime());
		Mat4x4 translation_matrix = glm::translate(Mat4x4(1.0f), Vec3(translation.x, translation.y, translation.z));

		Quat rotation = keyframes.GetInterpolatedRotation(animation.GetRunningTime());
		Mat4x4 rotation_matrix = glm::toMat4(rotation);

		Vec3 scale = keyframes.GetInterpolatedScale(animation.GetRunningTime());
		Mat4x4 scale_matrix = glm::scale(Mat4x4(1.0f), Vec3(scale.x, scale.y, scale.z));

		Mat4x4 nodeTransform = translation_matrix * rotation_matrix * scale_matrix;

		global_transform = parentTransform * nodeTransform;

		boneWSTransformMatrices[inBone.index] = global_transform;
		boneTransformMatrices[inBone.index] = global_transform * boneOffsetMatrices[inBone.index];
	}
	else if (inBone.index != rootBone.index && !has_animation)
	{

	}

	for (Bone& child_bone : inBone.children)
		UpdateBoneTransform(animation, child_bone, global_transform);

}



void DDGISceneSettings::FitToScene(const Scene& inScene, Transform& ioTransform)
{
	BBox3D scene_bounds;

	for (const auto& [entity, transform, mesh] : inScene.Each<Transform, Mesh>())
		scene_bounds.Combine(mesh.bbox.Transformed(transform.worldTransform));

	if (scene_bounds.IsValid())
	{
		ioTransform.position = scene_bounds.GetMin();
		ioTransform.Compose();

		mDDGIProbeSpacing = scene_bounds.GetExtents() / Vec3(mDDGIProbeCount);
	}
}


void Skeleton::UpdateFromAnimation(const Animation& animation)
{
	Mat4x4 matrix = Mat4x4(1.0f);
	UpdateBoneTransform(animation, rootBone, matrix);
}


void RigidBody::CreateBody(Physics& inPhysics, const Transform& inTransform)
{
    auto settings = JPH::BodyCreationSettings(
        shapeSettings,
        JPH::Vec3(inTransform.position.x, inTransform.position.y, inTransform.position.z),
        JPH::Quat(inTransform.rotation.x, inTransform.rotation.y, inTransform.rotation.z, inTransform.rotation.w),
        motionType,
        EPhysicsObjectLayers::MOVING
    );

    if (shapeSettings->GetRTTI() != JPH_RTTI(JPH::MeshShapeSettings))
        settings.mAllowDynamicOrKinematic = true;

    bodyID = inPhysics.GetSystem()->GetBodyInterface().CreateBody(settings)->GetID();
}


void RigidBody::ActivateBody(Physics& inPhysics, const Transform& inTransform)
{
    if (!bodyID.IsInvalid())
    {
        const JPH::Vec3 position = JPH::Vec3(inTransform.position.x, inTransform.position.y, inTransform.position.z);
        const JPH::Quat rotation = JPH::Quat(inTransform.rotation.x, inTransform.rotation.y, inTransform.rotation.z, inTransform.rotation.w);
        inPhysics.GetSystem()->GetBodyInterface().AddBody(bodyID, JPH::EActivation::Activate);
        inPhysics.GetSystem()->GetBodyInterface().SetPositionAndRotationWhenChanged(bodyID, position, rotation, JPH::EActivation::Activate);
    }
}


void RigidBody::DeactivateBody(Physics& inPhysics)
{
    inPhysics.GetSystem()->GetBodyInterface().RemoveBody(bodyID);
}


void RigidBody::CreateCubeCollider(Physics& inPhysics, const BBox3D& inBBox)
{
	const Vec3 half_extent = inBBox.GetExtents() / 2.0f;
	shapeSettings = new JPH::BoxShapeSettings(JPH::Vec3Arg(half_extent.x, half_extent.y, half_extent.z));
}


void RigidBody::CreateSphereCollider(Physics& inPhysics, float radius)
{
    shapeSettings = new JPH::SphereShapeSettings(radius);
}


void RigidBody::CreateMeshCollider(Physics& inPhysics, const Mesh& inMesh, const Transform& inTransform)
{
	JPH::TriangleList triangles;

	for (int i = 0; i < inMesh.indices.size(); i += 3)
	{
		Vec3 v0 = inMesh.positions[inMesh.indices[i]];
		Vec3 v1 = inMesh.positions[inMesh.indices[i + 1]];
		Vec3 v2 = inMesh.positions[inMesh.indices[i + 2]];

		v0 *= inTransform.GetScaleWorldSpace();
		v1 *= inTransform.GetScaleWorldSpace();
		v2 *= inTransform.GetScaleWorldSpace();

		triangles.push_back(JPH::Triangle(JPH::Float3(v0.x, v0.y, v0.z), JPH::Float3(v1.x, v1.y, v1.z), JPH::Float3(v2.x, v2.y, v2.z)));
	}

	shapeSettings = new JPH::MeshShapeSettings(triangles);
}


void RigidBody::CreateCylinderCollider(Physics& inPhysics, const Mesh& inMesh, const Transform& inTransform)
{
	const BBox3D aabb = BBox3D(inMesh.bbox).Scale(inTransform.scale);
	const Vec3 half_extent = aabb.GetExtents() / 2.0f;
	shapeSettings = new JPH::CylinderShapeSettings(half_extent.y, half_extent.x);
}

} // raekor