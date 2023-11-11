#pragma once

#include "ecs.h"
#include "util.h"

namespace Raekor {

class Ray;
class Assets;
struct Mesh;
struct Skeleton;
struct Material;
class IRenderInterface;
class NativeScript;
class SceneImporter;
struct DirectionalLight;


class Scene : public ecs::ECS
{
public:
	Scene(IRenderInterface* inRenderer) : m_Renderer(inRenderer) {}
	Scene(const Scene& inOther) = delete;
	~Scene() { Clear(); }

	// Spatial entity management
	Entity PickSpatialEntity(const Ray& inRay);
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
	void SaveToFile(const std::string& inFile, Assets& ioAssets);
	void OpenFromFile(const std::string& inFile, Assets& ioAssets);

	void BindScriptToEntity(Entity inEntity, NativeScript& inScript);

	void Optimize();

protected:
	Path m_ActiveSceneFilePath;
	IRenderInterface* m_Renderer;
	std::stack<Entity> m_Nodes;
};


class Importer
{
public:
	Importer(Scene& inScene, IRenderInterface* inRenderer) : m_Scene(inScene), m_Renderer(inRenderer) {}
	virtual bool LoadFromFile(const std::string& inFile, Assets* inAssets) = 0;

protected:
	Scene& m_Scene;
	IRenderInterface* m_Renderer = nullptr;
};


class SceneImporter : public Importer
{
public:
	SceneImporter(Scene& inScene, IRenderInterface* inRenderer) : Importer(inScene, inRenderer), m_ImportedScene(inRenderer) {}
	bool LoadFromFile(const std::string& inFile, Assets* inAssets) override;

private:
	void ParseNode(Entity inEntity, Entity inParent);
	void ConvertMesh(Entity inEntity, const Mesh& inMesh);
	void ConvertBones(Entity inEntity, const Skeleton& inSkeleton);
	void ConvertMaterial(Entity inEntity, const Material& inAssimpMaterial);

private:
	Scene m_ImportedScene;
	std::vector<Entity> m_CreatedNodeEntities;
	std::unordered_map<Entity, Entity> m_MaterialMapping;
};

} // Namespace Raekor