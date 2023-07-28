#pragma once

#include "util.h"

namespace Raekor {

class Ray;
class Assets;
class IRenderer;
class NativeScript;

using Entity = entt::entity;
static constexpr entt::null_t sInvalidEntity = entt::null;

class Scene : public entt::registry {
public:
	Scene(IRenderer* inRenderer) : m_Renderer(inRenderer) {}
	Scene(const Scene& inOther) = delete;
	~Scene() { clear(); }

	// Spatial entity management
	entt::entity	PickSpatialEntity(Ray& inRay);
	entt::entity	CreateSpatialEntity(const std::string& inName = "");
	void			DestroySpatialEntity(entt::entity inEntity);

	// TODO: kinda hacky, pls fix
	glm::vec3 GetSunLightDirection() const;

	// Per frame systems
	void UpdateLights();
	void UpdateTransforms();
	void UpdateAnimations(float inDeltaTime);
	void UpdateNativeScripts(float inDeltaTime);

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

} // Namespace Raekor