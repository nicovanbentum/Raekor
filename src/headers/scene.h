#pragma once

#include "ecs.h"
#include "components.h"
#include "rmath.h"
#include "assets.h"

namespace Raekor {

class Scene : public entt::registry {
public:
	Scene();
	~Scene();
	Scene(const Scene& rhs) = delete;

	// object management
	entt::entity	createObject(const std::string& name = "Empty");
	void			destroyObject(entt::entity entity);
	entt::entity	pickObject(Math::Ray& ray);

	entt::entity createDirectionalLight() {
		if (size<ecs::DirectionalLightComponent>() > 0) return entt::null;

		auto entity = createObject("Directional Light");
		auto& transform = get<ecs::TransformComponent>(entity);

		transform.rotation.x = static_cast<float>(M_PI / 12);
		transform.compose();

		emplace<ecs::DirectionalLightComponent>(entity);

		return entity;
	}

	// per frame systems
	void updateNode(entt::entity node, entt::entity parent);
	void updateTransforms();
	void loadMaterialTextures(const std::vector<entt::entity>& materials, AssetManager& assetManager);

	// save to disk
	void saveToFile(const std::string& file);
	void openFromFile(const std::string& file, AssetManager& assetManager);
};

} // Namespace Raekor