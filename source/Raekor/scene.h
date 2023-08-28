#pragma once

#include "ecs.h"
#include "util.h"

namespace Raekor {

class Ray;
class Assets;
struct Mesh;
struct Skeleton;
struct Material;
class IRenderer;
class NativeScript;
class SceneImporter;
struct DirectionalLight;


class Scene : public ecs::ECS {
public:
	Scene(IRenderer* inRenderer) : m_Renderer(inRenderer) {}
	Scene(const Scene& inOther) = delete;
	~Scene() { Clear(); }

	// Spatial entity management
	Entity PickSpatialEntity(Ray& inRay);
	Entity CreateSpatialEntity(const std::string& inName = "");
	void DestroySpatialEntity(Entity inEntity);

	// TODO: kinda hacky, pls fix
	Vec3 GetSunLightDirection() const;
	const DirectionalLight* GetSunLight() const;

	// Per frame systems
	void UpdateLights();
	void UpdateTransforms();
	void UpdateAnimations(float inDeltaTime);
	void UpdateNativeScripts(float inDeltaTime);

	// Entity operations
	Entity Clone(Entity inEntity);

	/* Loads materials from disk in parallel, is used for both importing and scene loading. */
	void LoadMaterialTextures(Assets& ioAssets, const Slice<Entity>& inMaterials);

	// save Scene to disk
	void SaveToFile(Assets& ioAssets, const std::string& inFile);
	void OpenFromFile(Assets& ioAssets, const std::string& inFile);

	void BindScriptToEntity(Entity inEntity, NativeScript& inScript);

private:
	IRenderer* m_Renderer;
	std::stack<Entity> m_Nodes;
};


class SceneImporter {
public:
	SceneImporter(Scene& inScene, IRenderer* inRenderer) : m_OutputScene(inScene), m_ImportedScene(inRenderer), m_Renderer(inRenderer) {}
	bool LoadFromFile(const std::string& inFile, Assets* inAssets);

private:
	void ParseNode(Entity inEntity, Entity inParent);
	void ConvertMesh(Entity inEntity, const Mesh& inMesh);
	void ConvertBones(Entity inEntity, const Skeleton& inSkeleton);
	void ConvertMaterial(Entity inEntity, const Material& inAssimpMaterial);

private:
	Scene& m_OutputScene;
	Scene m_ImportedScene;
	IRenderer* m_Renderer = nullptr;
	std::vector<Entity> m_CreatedNodeEntities;
	std::unordered_map<Entity, Entity> m_MaterialMapping;
};

} // Namespace Raekor