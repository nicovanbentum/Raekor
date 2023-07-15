#pragma once

#include "ecs.h"
#include "components.h"
#include "rmath.h"
#include "assets.h"
#include "physics.h"

namespace Raekor {

class IRenderer;

using Entity = entt::entity;
static constexpr entt::null_t sInvalidEntity = entt::null;

class Scene : public entt::registry {
public:
	Scene(IRenderer* inRenderer) : m_Renderer(inRenderer) {}
	Scene(const Scene& inOther) = delete;
	~Scene() { clear(); }

	// Spatial entity management
	entt::entity	PickSpatialEntity(Ray& inRay);
	entt::entity	CreateSpatialEntity(const std::string& name = "");
	void			DestroySpatialEntity(entt::entity entity);

	// TODO: kinda hacky, pls fix
	glm::vec3 GetSunLightDirection() const;

	// Per frame systems
	void UpdateLights();
	void UpdateTransforms();
	void UpdateAnimations(float inDeltaTime);
	void UpdateNativeScripts(float inDeltaTime);

	Entity Clone(Entity inEntity);

	/* Loads materials from disk in parallel, is used for both importing and scene loading. */
	void LoadMaterialTextures(Assets& assets, const Slice<Entity>& materials);

	// save Scene to disk
	void SaveToFile(Assets& assets, const std::string& file);
	void OpenFromFile(Assets& assets, const std::string& file);

	void BindScriptToEntity(entt::entity entity, NativeScript& script);

private:
	IRenderer* m_Renderer;
};

} // Namespace Raekor