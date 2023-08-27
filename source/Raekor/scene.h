#pragma once

#include "ecs.h"
#include "util.h"

namespace Raekor {

class Ray;
class Assets;
class IRenderer;
class NativeScript;
struct DirectionalLight;

class Scene : public ecs::ECS {
public:
	Scene(IRenderer* inRenderer) : m_Renderer(inRenderer) {}
	Scene(const Scene& inOther) = delete;
	~Scene() { Clear(); }

	// Spatial entity management
	Entity		PickSpatialEntity(Ray& inRay);
	Entity 	CreateSpatialEntity(const std::string& inName = "");
	void			DestroySpatialEntity(Entity inEntity);

	// TODO: kinda hacky, pls fix
	Vec3 GetSunLightDirection() const;
	const DirectionalLight* GetSunLight() const;

	// Per frame systems
	void UpdateLights();
	void UpdateTransforms();
	void UpdateAnimations(float inDeltaTime);
	void UpdateNativeScripts(float inDeltaTime);

	Entity Clone(Entity inEntity);

	void Merge(const Scene& inScene);

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

} // Namespace Raekor