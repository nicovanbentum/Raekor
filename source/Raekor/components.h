#pragma once

#include "ecs.h"
#include "rtti.h"
#include "rmath.h"
#include "camera.h"
#include "defines.h"
#include "animation.h"

namespace RK {

class Scene;
class ScriptAsset;
class TextureAsset;
class INativeScript;


struct Component 
{
	RTTI_DECLARE_TYPE(Component);
};


struct Name
{
	RTTI_DECLARE_TYPE(Name);

	Name() = default;
	Name(const char* inName) : name(inName) {}

	String name;

	operator const std::string& ( ) { return name; }
	bool operator==(const std::string& rhs) { return name == rhs; }
	Name& operator=(const std::string& rhs) { name = rhs; return *this; }
};


struct Transform
{
	RTTI_DECLARE_TYPE(Transform);

	Vec3 scale = Vec3(1.0f, 1.0f, 1.0f);
	Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
	Quat rotation = Quat(Vec3(0.0f, 0.0f, 0.0f));

	Mat4x4 localTransform = Mat4x4(1.0f);
	Mat4x4 worldTransform = Mat4x4(1.0f);
	Mat4x4 invWorldTransform = Mat4x4(1.0f);

	Vec3 GetScaleWorldSpace() const;
	Vec3 GetPositionWorldSpace() const;
	Quat GetRotationWorldSpace() const;

	void Print();
	void Compose();
	void Decompose();
};


struct DirectionalLight
{
	RTTI_DECLARE_TYPE(DirectionalLight);

	const Vec4& GetColor() const { return colour; }
	const Vec4& GetDirection() const { return direction; }


	glm::vec4 direction = { 0.25f, -0.9f, 0.0f, 0.0f };
	glm::vec4 colour = { 1.0f, 1.0f, 1.0f, 1.0f };
};


enum ELightType
{
	LIGHT_TYPE_NONE = 0,
	LIGHT_TYPE_SPOT,
	LIGHT_TYPE_POINT,
	LIGHT_TYPE_COUNT
};
RTTI_DECLARE_ENUM(ELightType);


struct Light
{
	RTTI_DECLARE_TYPE(Light);

	ELightType type = LIGHT_TYPE_NONE;
	Vec3 direction = { 0.0f, -1.0f, 0.0f }; // used for Directional and Spot light
	Vec4 position = { 0.0f, 0.0f, 0.0f, 0.0f }; // Used for Spot and Point light
	Vec4 colour = { 1.0f, 1.0f, 1.0f, 1.0f }; // rgb = color, a = intensity
	Vec4 attributes = { 1.0f, 0.0f, 0.0f, 0.0f }; // radius/range, spot inner cone angle, spot outer cone angle
};


struct MeshletTriangle
{
	uint32_t mX : 10;
	uint32_t mY : 10;
	uint32_t mZ : 10;
	uint32_t mW : 2;
};


struct Meshlet
{
	/* offsets within meshlet_vertices and meshlet_triangles arrays with meshlet data */
	uint32_t mVertexOffset;
	uint32_t mTriangleOffset;

	/* number of vertices and triangles used in the meshlet; data is stored in consecutive range defined by offset and count */
	uint32_t mVertexCount;
	uint32_t mTriangleCount;
};


struct Mesh
{
	RTTI_DECLARE_TYPE(Mesh);

	Array<Vec3> positions; // ptr
	Array<Vec2> uvs; // ptr
	Array<Vec3> normals; // ptr
	Array<Vec3> tangents; // ptr

	Array<uint32_t> indices; // ptr

	Array<Meshlet> meshlets;
	Array<uint32_t> meshletIndices;
	Array<MeshletTriangle> meshletTriangles;

	// GPU resources
	uint32_t vertexBuffer = 0;
	uint32_t indexBuffer = 0;
	uint32_t BottomLevelAS = 0;

	StaticArray<glm::vec3, 2> aabb;

	Entity material = Entity::Null;

	float mLODFade = 0.0f;

	Array<float> mInterleavedVertices;

	void CalculateAABB();
	void CalculateNormals();
	void CalculateTangents(float inTangentSign = 1.0f);

	uint32_t GetInterleavedStride() const;
	const std::vector<float>& GetInterleavedVertices();

	void Clear()
	{
		positions.clear();
		uvs.clear();
		normals.clear();
		tangents.clear();
		indices.clear();

	}
};


struct RigidBody
{
	RTTI_DECLARE_TYPE(RigidBody);

	JPH::BodyID bodyID;
	JPH::EMotionType motionType;
	JPH::BoxShapeSettings settings;
	JPH::MeshShapeSettings meshSettings;
	JPH::Ref<JPH::ShapeSettings> shapeSettings;

	void CreateCubeCollider(const BBox3D& inBBox);
	void CreateMeshCollider(const Mesh& inMesh, const Transform& inTransform);
	void CreateCylinderCollider(const Mesh& inMesh, const Transform& inTransform);
};


struct SoftBody
{
	RTTI_DECLARE_TYPE(SoftBody);

	JPH::BodyID mBodyID;
	JPH::SoftBodySharedSettings mSharedSettings;
	JPH::SoftBodyCreationSettings mCreationSettings;

	void CreateFromMesh(const Mesh& inMesh) { /* todo impl */ }
};



struct Skeleton
{
	RTTI_DECLARE_TYPE(Skeleton);

	struct Bone
	{
		RTTI_DECLARE_TYPE(Skeleton::Bone);

		uint32_t index;
		std::string name;
		std::vector<Bone> children;
	};
	
	Bone rootBone;
	Entity animation;

	Mat4x4 inverseGlobalTransform;
	Array<Vec4> boneWeights; // ptr
	Array<IVec4> boneIndices; // ptr
	Array<Mat4x4> boneOffsetMatrices; // ptr
	Array<Mat4x4> boneTransformMatrices; // ptr
	Array<Mat4x4> boneWSTransformMatrices; // ptr

	// Skinning GPU buffers
	uint32_t boneIndexBuffer;
	uint32_t boneWeightBuffer;
	uint32_t boneTransformsBuffer;
	uint32_t skinnedVertexBuffer;

	void DebugDraw(const Bone& inBone, const Mat4x4& inTransform);

	void UpdateFromAnimation(const Animation& animation);
	void UpdateBoneTransform(const Animation& inAnimation, Bone& inBone, const Mat4x4& inTransform);
};


struct Material
{
	RTTI_DECLARE_TYPE(Material);

	struct Texture 
	{
		RTTI_DECLARE_TYPE(Material::Texture);

		uint8_t swizzle;
		uint32_t gpuMap;
		String filepath;
	};

	enum TextureIndex
	{
		ALBEDO_INDEX, 
		NORMALS_INDEX, 
		EMISSIVE_INDEX, 
		METALLIC_INDEX, 
		ROUGHNESS_INDEX,
		TEXTURE_INDEX_COUNT
	};

	// properties
	Vec4 albedo = Vec4(1.0f);
	Vec3 emissive = Vec3(0.0f);
	float metallic = 0.0f;
	float roughness = 1.0f;
	bool isTransparent = false;

	StaticArray<Texture, TEXTURE_INDEX_COUNT> textures;

	Texture& GetAlbedoTexture() { return textures[ALBEDO_INDEX]; }
	const Texture& GetAlbedoTexture() const { return textures[ALBEDO_INDEX]; }

	Texture& GetNormalsTexture() { return textures[NORMALS_INDEX]; }
	const Texture& GetNormalsTexture() const { return textures[NORMALS_INDEX]; }

	Texture& GetEmissiveTexture() { return textures[EMISSIVE_INDEX]; }
	const Texture& GetEmissiveTexture() const { return textures[EMISSIVE_INDEX]; }

	Texture& GetMetallicTexture() { return textures[METALLIC_INDEX]; }
	const Texture& GetMetallicTexture() const { return textures[METALLIC_INDEX]; }

	Texture& GetRoughnessTexture() { return textures[ROUGHNESS_INDEX]; }
	const Texture& GetRoughnessTexture() const { return textures[ROUGHNESS_INDEX]; }

	// texture file paths
	String albedoFile; // ptr
	String normalFile;  // ptr
	String emissiveFile; // ptr
	String metallicFile; // ptr
	String roughnessFile;  // ptr

	String vertexShaderFile; // ptr
	String pixelShaderFile; // ptr

	uint64_t vertexShader = 0;
	uint64_t pixelShader = 0;

	// GPU resources
	uint32_t gpuAlbedoMap = 0;
	uint32_t gpuNormalMap = 0;
	uint32_t gpuMetallicMap = 0;
	uint32_t gpuEmissiveMap = 0;
	uint32_t gpuRoughnessMap = 0;
	uint8_t gpuAlbedoMapSwizzle = TEXTURE_SWIZZLE_RGBA;
	uint8_t gpuNormalMapSwizzle = TEXTURE_SWIZZLE_RGBA;
	uint8_t gpuEmissiveMapSwizzle = TEXTURE_SWIZZLE_RGBA;
	uint8_t gpuMetallicMapSwizzle = TEXTURE_SWIZZLE_BBBB; // assume GLTF by default, TODO: make changes in FbxImporter?
	uint8_t gpuRoughnessMapSwizzle = TEXTURE_SWIZZLE_GGGG; // assume GLTF by default, TODO: make changes in FbxImporter?

	bool IsLoadedNew() const { for (const Texture& texture : textures) if (texture.gpuMap == 0) return false; return true; }

	bool IsLoaded() const { return gpuAlbedoMap != 0 && gpuNormalMap != 0 && gpuMetallicMap != 0 && gpuRoughnessMap != 0; }

	// default material for newly spawned meshes
	static Material Default;
};


struct NativeScript
{
	RTTI_DECLARE_TYPE(NativeScript);

	String file; // ptr
	String type;
	Array<String> types;
	INativeScript* script = nullptr;
};


struct DDGISceneSettings
{
	RTTI_DECLARE_TYPE(DDGISceneSettings);

	void FitToScene(const Scene& inScene, Transform& ioTransform);

	IVec3 mDDGIDebugProbe = UVec3(10, 10, 5);
	IVec3 mDDGIProbeCount = UVec3(22, 22, 22);
	Vec3 mDDGIProbeSpacing = Vec3(0.1f, 0.1f, 0.1f);
};


template<typename Component>
struct ComponentDescription
{
	const char* name;
	using type = Component;
};


static constexpr auto Components = std::make_tuple(
	ComponentDescription<Name>              {"Name"},
	ComponentDescription<Transform>         {"Transform"},
	ComponentDescription<Mesh>              {"Mesh"},
	ComponentDescription<SoftBody>          {"Soft Body"},
	ComponentDescription<Material>          {"Material"},
	ComponentDescription<Light>				{"Light"},
	ComponentDescription<DirectionalLight>  {"Directional Light"},
	ComponentDescription<RigidBody>			{"Rigid Body"},
	ComponentDescription<Skeleton>          {"Skeleton"},
	ComponentDescription<NativeScript>      {"Native Script"},
	ComponentDescription<DDGISceneSettings> {"DDGI Scene Settings"},
	ComponentDescription<Animation>			{"Animation"}
);

void gRegisterComponentTypes();

} // Raekor
