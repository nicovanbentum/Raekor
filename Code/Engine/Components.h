#pragma once

#include "PCH.h"
#include "ECS.h"
#include "RTTI.h"
#include "Maths.h"
#include "Camera.h"
#include "Defines.h"
#include "Assets.h"
#include "Animation.h"

namespace RK {

class Scene;
class Physics;
class TextureAsset;
class INativeScript;
class IRenderInterface;
class IComponentStorage;

struct Name
{
	RTTI_DECLARE_TYPE(Name);

	Name() = default;
	Name(StringView inName) : name(inName) {}
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
    Mat4x4 prevWorldTransform = Mat4x4(1.0f);

	Entity animation = Entity::Null;
	String animationChannel = "";

	Vec3 GetScaleWorldSpace() const;
	Vec3 GetPositionWorldSpace() const;
	Quat GetRotationWorldSpace() const;

    void Scale(Vec3 inScale);
    void Translate(Vec3 inTranslation);

    void Rotate(Quat inRotation);
    void Rotate(float inDegrees, Vec3 inAxis);

	void Print();
	void Compose();
	void Decompose();
};


struct DirectionalLight
{
	RTTI_DECLARE_TYPE(DirectionalLight);

	static constexpr Vec3 cDefaultDirection = Vec3(0.25f, -0.9f, 0.0f);

	Vec4 GetColor() const { return Vec4(Vec3(color), illuminance); }
	const Vec4& GetDirection() const { return direction; }

	String cubeMapFile;
	uint32_t cubeMap = 0;

	Vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vec4 direction = { 0.25f, -0.9f, 0.0f, 0.0f };
	float illuminance = 110000.0f;

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
	Vec3 direction = { 0.0f, -1.0f, 0.0f }; // runtime property, used for Directional and Spot light
	Vec4 position = { 0.0f, 0.0f, 0.0f, 0.0f }; // runtime property, used for Spot and Point light
	Vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f }; // rgb = color, a = intensity
	Vec4 attributes = { 1.0f, 0.1f, 0.0f, 0.0f }; // x = radius (point), y = range (spot), z = inner cone angle, w = outer cone angle
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


struct MeshGeometry
{
	String name;
	BBox3D bbox;

	Array<Vec3> positions;
	Array<Vec2> uvs;
	Array<Vec3> normals;
	Array<Vec3> tangents;

	Array<uint32_t> indices;
	Array<float> vertices; // OPTIMIZE ME

	uint32_t vertexBuffer = 0;
	uint32_t indexBuffer = 0;
	uint32_t BottomLevelAS = 0;

	static void CreateCube(MeshGeometry& ioMesh, float inScale);
	static void CreatePlane(MeshGeometry& ioMesh, float inScale);
	static void CreateSphere(MeshGeometry& ioMesh, float inRadius, uint32_t inSectorCount, uint32_t inStackCount);

	void CalculateNormals();
	void CalculateTangents();
	void CalculateVertices();
	void CalculateBoundingBox();

	uint32_t GetVertexStride() const;

	bool IsEmpty() const { return positions.empty() || indices.empty(); }
	bool IsUploaded() const { return vertexBuffer != 0 && indexBuffer != 0 && BottomLevelAS != 0; }
};


class MeshAsset : public Asset
{
public:
	RTTI_DECLARE_VIRTUAL_TYPE(MeshAsset);

	using Ptr = SharedPtr<MeshAsset>;

	static constexpr uint32_t cMagicNumber = 0x534D4B52;
	static constexpr uint32_t cVersion = 1;
	static constexpr const char* cBuiltinCube = "Builtin/Cube";
	static constexpr const char* cBuiltinPlane = "Builtin/Plane";
	static constexpr const char* cBuiltinSphere = "Builtin/Sphere";

	MeshAsset() = default;
	MeshAsset(const String& inPath) : Asset(inPath) {}
	virtual ~MeshAsset();

	virtual bool Load() override;
	bool Save(const String& inPath) const;

	static String GetCachedPath(const String& inSource) { return Asset::GetCachedPath(inSource, ".meshes"); }
	static String GetSourcePath(const Path& inFile);
	static bool IsBuiltin(const String& inSource) { return inSource.starts_with("Builtin/"); }

	static Ptr Create(const String& inSource);
	static Ptr CreateBuiltin(const String& inSource);
	static Ptr Import(const String& inSource);
	static Ptr Find(Assets& ioAssets, const String& inSource);

	const String& GetSource() const { return m_Source; }

	uint32_t GetMeshCount() const { return uint32_t(m_Meshes.size()); }
	MeshGeometry* GetMesh(uint32_t inIndex) { return inIndex < m_Meshes.size() ? &m_Meshes[inIndex] : nullptr; }
	const MeshGeometry* GetMesh(uint32_t inIndex) const { return inIndex < m_Meshes.size() ? &m_Meshes[inIndex] : nullptr; }

	Array<MeshGeometry>& GetMeshes() { return m_Meshes; }
	const Array<MeshGeometry>& GetMeshes() const { return m_Meshes; }

	IRenderInterface* GetRenderer() const { return m_Renderer; }
	void SetRenderer(IRenderInterface* inRenderer) { m_Renderer = inRenderer; }

private:
	String m_Source;
	Array<MeshGeometry> m_Meshes;
	IRenderInterface* m_Renderer = nullptr;
};


struct Mesh
{
	RTTI_DECLARE_TYPE(Mesh);

	String source;
	uint32_t index = 0;
	Entity material = Entity::Null;
	float lodFade = 0.0f;

	MeshAsset::Ptr asset;

	void SetAsset(const MeshAsset::Ptr& inAsset, uint32_t inIndex);

	MeshGeometry* GetGeometry() const { return asset ? asset->GetMesh(index) : nullptr; }
	BBox3D GetBounds() const;

	bool IsLoaded() const;
};


struct RigidBody
{
	RTTI_DECLARE_TYPE(RigidBody);

    enum Shape 
    {
        NONE = 0, 
        CUBE, 
        SPHERE, 
        MESH, 
        CYLINDER
    };

    enum Motion
    {
        STATIC,
        DYNAMIC,
        KINEMATIC
    };

    Shape shape = NONE;
    Motion motion = STATIC;
    Array<uint8_t> shapeData;

    BBox3D cubeBounds;
    float sphereRadius;

	JPH::BodyID bodyID;
	JPH::EMotionType motionType = JPH::EMotionType::Static;
    JPH::BoxShapeSettings settings;
    JPH::MeshShapeSettings meshSettings;
	JPH::Ref<JPH::ShapeSettings> shapeSettings;

    void CreateBody(Physics& inPhysics, const Transform& inTransform);
    void ActivateBody(Physics& inPhysics, const Transform& inTransform);
    void DeactivateBody(Physics& inPhysics);
    void DestroyBody(Physics& inPhysics);

    void CreateCubeCollider(Physics& inPhysics, const BBox3D& inBBox);
    void CreateSphereCollider(Physics& inPhysics, float inRadius);

	void CreateMeshCollider(Physics& inPhysics, const MeshGeometry& inGeometry, const Transform& inTransform);
	void CreateCylinderCollider(Physics& inPhysics, const MeshGeometry& inGeometry, const Transform& inTransform);
};


struct SoftBody
{
	RTTI_DECLARE_TYPE(SoftBody);

	JPH::BodyID mBodyID;
	JPH::SoftBodySharedSettings mSharedSettings;
	JPH::SoftBodyCreationSettings mCreationSettings;

	void CreateFromMesh(const MeshGeometry& inGeometry) { /* todo impl */ }
};



struct Skeleton
{
	RTTI_DECLARE_TYPE(Skeleton);

	struct Bone
	{
		RTTI_DECLARE_TYPE(Skeleton::Bone);

		uint32_t index;
		String name;
		Array<Bone> children;
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
	uint32_t boneIndexBuffer = 0;
	uint32_t boneWeightBuffer = 0;
	uint32_t boneTransformsBuffer = 0;
	uint32_t skinnedVertexBuffer = 0;
	uint32_t skinnedBottomLevelAS = 0;
	uint32_t blasScratchBuffer = 0;
	bool gpuBuffersUploaded = false;

	void DebugDraw(const Bone& inBone, const Mat4x4& inTransform);

	void UpdateFromAnimation(const Animation& animation);
	void UpdateBoneTransform(const Animation& inAnimation, Bone& inBone, const Mat4x4& inTransform);
};


enum EMaterialBlendMode : uint8_t
{
	MATERIAL_BLEND_MODE_OPAQUE = 0,
	MATERIAL_BLEND_MODE_MASKED,
	MATERIAL_BLEND_MODE_BLENDED,
	MATERIAL_BLEND_MODE_COUNT
};
RTTI_DECLARE_ENUM(EMaterialBlendMode);


enum EMaterialSamplerFilter : uint8_t
{
	MATERIAL_SAMPLER_FILTER_POINT = 0,
	MATERIAL_SAMPLER_FILTER_LINEAR,
	MATERIAL_SAMPLER_FILTER_ANISOTROPIC,
	MATERIAL_SAMPLER_FILTER_COUNT
};
RTTI_DECLARE_ENUM(EMaterialSamplerFilter);


enum EMaterialSamplerAddressMode : uint8_t
{
	MATERIAL_SAMPLER_ADDRESS_MODE_WRAP = 0,
	MATERIAL_SAMPLER_ADDRESS_MODE_CLAMP,
	MATERIAL_SAMPLER_ADDRESS_MODE_COUNT
};
RTTI_DECLARE_ENUM(EMaterialSamplerAddressMode);


struct Material
{
	RTTI_DECLARE_TYPE(Material);

	// properties
	Vec4 albedo = Vec4(1.0f);
	Vec3 emissive = Vec3(0.0f);
	float metallic = 0.0f;
	float roughness = 1.0f;
	float alphaCutoff = 0.5f;
	EMaterialBlendMode blendMode = MATERIAL_BLEND_MODE_OPAQUE;
	EMaterialSamplerFilter samplerFilter = MATERIAL_SAMPLER_FILTER_ANISOTROPIC;
	EMaterialSamplerAddressMode samplerAddressMode = MATERIAL_SAMPLER_ADDRESS_MODE_WRAP;

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
	uint8_t gpuSampler = 0;
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

	void UpdateSamplerIndex();

	bool IsLoaded() const { return gpuAlbedoMap != 0 && gpuNormalMap != 0 && gpuMetallicMap != 0 && gpuRoughnessMap != 0 && gpuEmissiveMap != 0; }

	// default material for newly spawned meshes
	static RK_API Material Default;
};


struct NativeScript
{
	RTTI_DECLARE_TYPE(NativeScript);

	String file; // ptr
	String type;
	String variables;
	INativeScript* script = nullptr;
};


enum ESoundCategory
{
	SOUND_CATEGORY_EFFECTS = 0,
	SOUND_CATEGORY_AMBIENCE,
	SOUND_CATEGORY_MUSIC,
	SOUND_CATEGORY_COUNT
};
RTTI_DECLARE_ENUM(ESoundCategory);


struct SoundSource
{
	RTTI_DECLARE_TYPE(SoundSource);

	String file;
	ESoundCategory category = SOUND_CATEGORY_EFFECTS;
	float volume = 1.0f;
	float pitch = 1.0f;
	bool loop = false;
	bool stream = false;

	bool playOnStart = false;
	bool playOnTriggerEnter = false;
	bool stopOnTriggerExit = false;
	float fadeInTime = 0.0f;
	float fadeOutTime = 0.0f;

	bool spatial = true;
	float minDistance = 1.0f;
	float maxDistance = 50.0f;
	float rolloff = 1.0f;
	float dopplerFactor = 1.0f;

	uint32_t voice = 0;
};


struct AudioListener
{
	RTTI_DECLARE_TYPE(AudioListener);
};


struct TriggerVolume
{
	RTTI_DECLARE_TYPE(TriggerVolume);

	Vec3 size = Vec3(2.0f);
	Entity activator = Entity::Null;
	bool once = false;

	bool inside = false;
	bool entered = false;
	bool exited = false;
	bool triggered = false;

	bool Contains(const Transform& inTransform, const Vec3& inPoint) const;
	void Reset() { inside = entered = exited = triggered = false; }
};


struct DDGISceneSettings
{
	RTTI_DECLARE_TYPE(DDGISceneSettings);

	void FitToScene(const Scene& inScene, Transform& ioTransform);

    bool mUseChebyshev = false;
	IVec3 mDDGIDebugProbe = UVec3(10, 10, 5);
	IVec3 mDDGIProbeCount = UVec3(22, 22, 22);
	Vec3 mDDGIProbeSpacing = Vec3(0.1f, 0.1f, 0.1f);
	bool mFollowCamera = false;
	int mCascadeCount = 1;
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
	ComponentDescription<Camera>            {"Camera"},
	ComponentDescription<SoftBody>          {"Soft Body"},
	ComponentDescription<Material>          {"Material"},
	ComponentDescription<Light>				{"Light"},
	ComponentDescription<DirectionalLight>  {"Directional Light"},
	ComponentDescription<RigidBody>			{"Rigid Body"},
	ComponentDescription<Skeleton>          {"Skeleton"},
	ComponentDescription<NativeScript>      {"Native Script"},
	ComponentDescription<DDGISceneSettings> {"DDGI Scene Settings"},
	ComponentDescription<Animation>			{"Animation"},
	ComponentDescription<SoundSource>       {"Sound Source"},
	ComponentDescription<AudioListener>     {"Audio Listener"},
	ComponentDescription<TriggerVolume>     {"Trigger Volume"}
);

void gRegisterComponentTypes();

} // Raekor
