#pragma once

#include "ecs.h"
#include "components.h"
#include "rmath.h"
#include "assets.h"

namespace Raekor {

class Async;

class Scene : public entt::registry {
public:
	Scene();
	~Scene();
	Scene(const Scene& rhs) = delete;

	// object management
	entt::entity	createObject(const std::string& name = "Empty");
	void			destroyObject(entt::entity entity);
	entt::entity	pickObject(Math::Ray& ray);

	// per frame systems
	void updateNode(entt::entity node, entt::entity parent);
	void updateTransforms();
	void updateLights();

	void loadMaterialTextures(Async& async, Assets& assets, const std::vector<entt::entity>& materials);

	// save to disk
	void saveToFile(const std::string& file);
	void openFromFile(Async& async, Assets& assets, const std::string& file);
};

} // Namespace Raekor