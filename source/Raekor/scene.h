#pragma once

#include "ecs.h"
#include "components.h"
#include "rmath.h"
#include "assets.h"
#include "physics.h"

namespace Raekor {

using Entity = entt::entity;
static constexpr entt::null_t sInvalidEntity = entt::null;

class Scene : public entt::registry {
public:
	Scene() = default;
	~Scene() { clear(); }
	Scene(const Scene& rhs) = delete;

	// Spatial entity management
	entt::entity	PickSpatialEntity(Math::Ray& ray);
	entt::entity	CreateSpatialEntity(const std::string& name);
	void			DestroySpatialEntity(entt::entity entity);

	// Per frame systems
	void UpdateSelfAndChildNodes(entt::entity node, entt::entity parent);
	void UpdateTransforms();
	void UpdateLights();

	/* Loads materials from disk in parallel, is used for both importing and scene loading. */
	void LoadMaterialTextures(Assets& assets, const std::vector<entt::entity>& materials);

	// save Scene to disk
	void SaveToFile(Assets& assets, const std::string& file);
	void OpenFromFile(Assets& assets, const std::string& file);

	template<typename Fn> void SetUploadMeshCallbackFunction(Fn&& fn) { m_UploadMeshCallback = fn; }
	template<typename Fn> void SetUploadMaterialCallbackFunction(Fn&& fn) { m_UploadMaterialCallback = fn; }
	
	void BindScriptToEntity(entt::entity entity, NativeScript& script);

private:
	std::function<void(Mesh&)> m_UploadMeshCallback = nullptr;
	std::function<void(Material&, Assets&)> m_UploadMaterialCallback = nullptr;
};

} // Namespace Raekor