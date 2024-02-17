#pragma once

#include "ecs.h"
#include "rtti.h"
#include "rmath.h"
#include "defines.h"
#include "animation.h"

namespace Raekor {

class Scene;
class ScriptAsset;
class TextureAsset;
class INativeScript;

struct SCRIPT_INTERFACE Name
{
	RTTI_DECLARE_TYPE(Name);

	std::string name;

	operator const std::string& ( ) { return name; }
	bool operator==(const std::string& rhs) { return name == rhs; }
	Name& operator=(const std::string& rhs) { name = rhs; return *this; }
};


struct SCRIPT_INTERFACE Transform
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


struct SCRIPT_INTERFACE DirectionalLight
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


struct SCRIPT_INTERFACE Light
{
	RTTI_DECLARE_TYPE(Light);

	ELightType type = LIGHT_TYPE_NONE;
	glm::vec3 direction = { 0.0f, -1.0f, 0.0f }; // used for Directional and Spot light
	glm::vec4 position = { 0.0f, 0.0f, 0.0f, 0.0f }; // Used for Spot and Point light
	glm::vec4 colour = { 1.0f, 1.0f, 1.0f, 1.0f }; // rgb = color, a = intensity
	glm::vec4 attributes = { 1.0f, 0.0f, 0.0f, 0.0f }; // radius/range, spot inner cone angle, spot outer cone angle
};


struct SCRIPT_INTERFACE Node
{
	RTTI_DECLARE_TYPE(Node);

	Entity parent = NULL_ENTITY;
	Entity firstChild = NULL_ENTITY;
	Entity prevSibling = NULL_ENTITY;
	Entity nextSibling = NULL_ENTITY;

	bool IsRoot() const { return parent == NULL_ENTITY; }
	bool HasChildren() const { return firstChild != NULL_ENTITY; }
	bool IsConnected() const { return firstChild != NULL_ENTITY && parent != NULL_ENTITY; }
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


struct SCRIPT_INTERFACE Mesh
{
	RTTI_DECLARE_TYPE(Mesh);

	std::vector<glm::vec3> positions; // ptr
	std::vector<glm::vec2> uvs; // ptr
	std::vector<glm::vec3> normals; // ptr
	std::vector<glm::vec3> tangents; // ptr

	std::vector<uint32_t> indices; // ptr

	std::vector<Meshlet> meshlets;
	std::vector<uint32_t> meshletIndices;
	std::vector<MeshletTriangle> meshletTriangles;

	// GPU resources
	uint32_t vertexBuffer = 0;
	uint32_t indexBuffer = 0;
	uint32_t BottomLevelAS = 0;

	std::array<glm::vec3, 2> aabb;

	Entity material = NULL_ENTITY;

	float mLODFade = 0.0f;

	std::vector<float> mInterleavedVertices;

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

struct BoxCollider
{
	RTTI_DECLARE_TYPE(BoxCollider);

	JPH::BodyID bodyID;
	JPH::EMotionType motionType;
	JPH::BoxShapeSettings settings;
	JPH::MeshShapeSettings meshSettings;

	void CreateFromMesh(const Mesh& inMesh);
};


struct SoftBody
{
	RTTI_DECLARE_TYPE(SoftBody);

	JPH::BodyID mBodyID;
	JPH::SoftBodySharedSettings mSharedSettings;
	JPH::SoftBodyCreationSettings mCreationSettings;

	void CreateFromMesh(const Mesh& inMesh) { /* todo impl */ }
};

struct Bone
{
	RTTI_DECLARE_TYPE(Bone);

	uint32_t index;
	std::string name;
	std::vector<Bone> children;
};


struct Skeleton
{
	RTTI_DECLARE_TYPE(Skeleton);

	glm::mat4 inverseGlobalTransform;
	std::vector<glm::vec4> boneWeights; // ptr
	std::vector<glm::ivec4> boneIndices; // ptr
	std::vector<glm::mat4> boneOffsetMatrices; // ptr
	std::vector<glm::mat4> boneTransformMatrices; // ptr
	std::vector<glm::mat4> boneWSTransformMatrices; // ptr

	Bone boneHierarchy;
	std::vector<Animation> animations; // ptr

	void DebugDraw(const Bone& inBone);
	void UpdateFromAnimation(Animation& animation, float TimeInSeconds);
	void UpdateBoneTransforms(const Animation& animation, float animationTime, Bone& pNode, const glm::mat4& parentTransform);

	// Skinning GPU buffers
	uint32_t boneIndexBuffer;
	uint32_t boneWeightBuffer;
	uint32_t boneTransformsBuffer;
	uint32_t skinnedVertexBuffer;

	// debug settings
	bool autoPlay = true;
	float currentTime = 0.0f;
	uint32_t animationIndex = 0;
};


struct SCRIPT_INTERFACE Material
{
	RTTI_DECLARE_TYPE(Material);

	// properties
	glm::vec4 albedo = glm::vec4(1.0f);
	glm::vec3 emissive = glm::vec3(0.0f);
	float metallic = 0.0f;
	float roughness = 1.0f;
	bool isTransparent = false;

	// texture file paths
	std::string albedoFile; // ptr
	std::string normalFile;  // ptr
	std::string emissiveFile; // ptr
	std::string metallicFile; // ptr
	std::string roughnessFile;  // ptr

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

	bool IsLoaded() const { return gpuAlbedoMap != 0 && gpuNormalMap != 0 && gpuMetallicMap != 0 && gpuRoughnessMap != 0; }

	// default material for newly spawned meshes
	static Material Default;
};


struct NativeScript
{
	RTTI_DECLARE_TYPE(NativeScript);

	std::string file; // ptr
	std::string type;
	std::vector<std::string> types;
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
	ComponentDescription<Node>              {"Node"},
	ComponentDescription<Transform>         {"Transform"},
	ComponentDescription<Mesh>              {"Mesh"},
	ComponentDescription<SoftBody>          {"Soft Body"},
	ComponentDescription<Material>          {"Material"},
	ComponentDescription<Light>				{"Light"},
	ComponentDescription<DirectionalLight>  {"Directional Light"},
	ComponentDescription<BoxCollider>       {"Box Collider"},
	ComponentDescription<Skeleton>          {"Skeleton"},
	ComponentDescription<NativeScript>      {"Native Script"},
	ComponentDescription<DDGISceneSettings> {"DDGI Scene Settings"}
);

void gRegisterComponentTypes();

} // Raekor
