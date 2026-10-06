#pragma once

#include "ecs.h"
#include "defines.h"

namespace RK {

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
	Entity CreateSpatialEntity(StringView inName = "");
	void DestroySpatialEntity(Entity inEntity);

	// TODO: kinda hacky, pls fix
	Vec3 GetSunLightDirection() const;
	const DirectionalLight* GetSunLight() const;

	// Per frame systems
	void UpdateLights();
	void UpdateCameras();
	void UpdateStreaming();
	void UpdateTransforms();
	void UpdateAnimations(float inDeltaTime);
	void UpdateNativeScripts(float inDeltaTime, Application* inApp);

	// debug stuff
	void RenderDebugShapes(Entity inEntity, float inOpacity) const;

	// entity hierarchy stuff
	using TraverseFunction = void(*)(void*, Scene&, Entity);
	void TraverseDepthFirst(Entity inEntity, TraverseFunction inFunction, void* inContext);
	void TraverseBreadthFirst(Entity inEntity, TraverseFunction inFunction, void* inContext);

	Entity GetRootEntity() const { return m_RootEntity; }

	bool HasChildren(Entity inEntity) const { return !m_Hierarchy.findRight(inEntity)->empty(); }
	Slice<const Entity> GetChildren(Entity inEntity) { return *m_Hierarchy.findRight(inEntity); }
	
	bool HasParent(Entity inEntity) const { return GetParent(inEntity) != Entity::Null; }
	Entity GetParent(Entity inEntity) const { return m_Hierarchy.findLeft(inEntity, Entity::Null); }

	void Unparent(Entity inEntity) { assert(inEntity != Entity::Null); m_Hierarchy.eraseRight(inEntity); }
	void ParentTo(Entity inEntity, Entity inParent) { assert(inEntity != Entity::Null && inParent != Entity::Null); m_Hierarchy.insert(inParent, inEntity); }

	// entity operations
	void Destroy(Entity inEntity);
	Entity Clone(Entity inEntity, Entity inParent);
	Entity Clone(Entity inEntity) { return Clone(inEntity, GetParent(inEntity)); }

	// load materials from disk in parallel, is used for both importing and scene loading.
	void LoadMaterialTextures(Assets& ioAssets);
	void LoadMaterialTextures(Assets& ioAssets, Slice<const Entity> inMaterials);

	// save Scene to disk
	void SaveToFile(const String& inFile, Assets& ioAssets, Application* inApp = nullptr);
	void OpenFromFile(const String& inFile, Assets& ioAssets, Application* inApp = nullptr);
	bool LoadFromFile(const String& inFile, Assets& ioAssets);

	void Swap(Scene& ioOther);
	void CopyFrom(Scene& inOther);
	Array<Entity> Merge(Scene& ioOther);

	void UploadMeshes();
	void UploadMeshes(Slice<const Entity> inEntities);
	void UploadDirectionalLightCubeMaps(Assets& ioAssets);

	void ReleaseResources();

	const Path& GetFilePath() const { return m_ActiveSceneFilePath; }
	void SetFilePath(const Path& inPath) { m_ActiveSceneFilePath = inPath; }

	// script utilities
	void BindScripts(Application* inApp);
	void UnbindScripts();
	void StoreScriptVariables();
	void BindScriptToEntity(Entity inEntity, NativeScript& inScript, Application* inApp);
	void Optimize();

private:
	bool ReadSceneFile(const String& inFilePath);

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
	virtual bool LoadFromFile(const String& inFile, Assets* inAssets) = 0;

protected:
	Scene& m_Scene;
	IRenderInterface* m_Renderer = nullptr;
};


class SceneImporter : public Importer
{
public:
	SceneImporter(Scene& inScene, IRenderInterface* inRenderer) : Importer(inScene, inRenderer), m_ImportedScene(nullptr) {}
	bool LoadFromFile(const String& inFile, Assets* inAssets) override;

private:
	Scene m_ImportedScene;
};

} // Namespace Raekor