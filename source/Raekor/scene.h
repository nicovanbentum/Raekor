#pragma once

#include "ecs.h"
#include "components.h"
#include "rmath.h"
#include "assets.h"

namespace Raekor {

class Async;

class Scene : public entt::registry {
public:
	Scene() = default;
	~Scene();
	Scene(const Scene& rhs) = delete;

	// object management
	entt::entity	createObject(const std::string& name = "Empty");
	void			destroyObject(entt::entity entity);
	entt::entity	pickObject(Math::Ray& ray);

	void bindScript(entt::entity entity, NativeScript& script);

	// per frame systems
	void updateNode(entt::entity node, entt::entity parent);
	void updateTransforms();
	void updateLights();

	void loadMaterialTextures(Assets& assets, const std::vector<entt::entity>& materials);

	// save to disk
	void saveToFile(Assets& assets, const std::string& file);
	void openFromFile(Assets& assets, const std::string& file);

	template<typename Fn>
	void SetUploadMeshCallbackFunction(Fn&& fn) { m_UploadMeshCallback = fn; }

	template<typename Fn>
	void SetUploadMaterialCallbackFunction(Fn&& fn) { m_UploadMaterialCallback = fn; }

private:
	std::function<void(Mesh&)> m_UploadMeshCallback = nullptr;
	std::function<void(Material&, Assets&)> m_UploadMaterialCallback = nullptr;
};

} // Namespace Raekor