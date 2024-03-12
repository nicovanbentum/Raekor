#pragma once

#include "ecs.h"
#include "defines.h"

namespace Raekor {

class Ray;
class Assets;
class Application;
class NativeScript;
class SceneImporter;
class IRenderInterface;

struct Mesh;
struct Material;
struct Skeleton;
struct DirectionalLight;


class Scene : public ECStorage
{
public:
	NO_COPY_NO_MOVE(Scene);

	Scene(IRenderInterface* inRenderer);
	~Scene() { Clear(); }

	// Spatial entity management
	Entity PickSpatialEntity(const Ray& inRay) const;
	Entity CreateSpatialEntity(std::string_view inName = "");
	void DestroySpatialEntity(Entity inEntity);

	// TODO: kinda hacky, pls fix
	Vec3 GetSunLightDirection() const;
	const DirectionalLight* GetSunLight() const;

	// Per frame systems
	void UpdateLights();
	void UpdateTransforms();
	void UpdateAnimations(float inDeltaTime);
	void UpdateNativeScripts(float inDeltaTime);

	// debug stuff
	void RenderDebugShapes(Entity inEntity) const;

	// entity hierarchy stuff
	using TraverseFunction = void(*)(void*, Scene&, Entity);
	void TraverseDepthFirst(Entity inEntity, TraverseFunction inFunction, void* inContext);
	void TraverseBreadthFirst(Entity inEntity, TraverseFunction inFunction, void* inContext);

	Entity GetRootEntity() const { return m_RootEntity; }

	bool HasChildren(Entity inEntity) const { return !m_Hierarchy.findRight(inEntity)->empty(); }
	Slice<Entity> GetChildren(Entity inEntity) { return *m_Hierarchy.findRight(inEntity); }
	
	bool HasParent(Entity inEntity) const { return GetParent(inEntity) != Entity::Null; }
	Entity GetParent(Entity inEntity) const { return m_Hierarchy.findLeft(inEntity, Entity::Null); }

	void Unparent(Entity inEntity) { assert(inEntity != Entity::Null); m_Hierarchy.eraseRight(inEntity); }
	void ParentTo(Entity inEntity, Entity inParent) { assert(inEntity != Entity::Null && inParent != Entity::Null); m_Hierarchy.insert(inParent, inEntity); }

	// entity operations
	Entity Clone(Entity inEntity);

	// load materials from disk in parallel, is used for both importing and scene loading.
	void LoadMaterialTextures(Assets& ioAssets, const Slice<Entity>& inMaterials);

	// save Scene to disk
	void SaveToFile(const std::string& inFile, Assets& ioAssets, Application* inApp = nullptr);
	void OpenFromFile(const std::string& inFile, Assets& ioAssets, Application* inApp = nullptr);

	// script utilities
	void BindScriptToEntity(Entity inEntity, NativeScript& inScript, Application* inApp);


	void Optimize();

protected:
	Path m_ActiveSceneFilePath;
	IRenderInterface* m_Renderer;
	
private:
	Entity m_RootEntity;
	std::stack<Entity> m_DFS;
	std::queue<Entity> m_BFS;
	EntityHierarchy m_Hierarchy;
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