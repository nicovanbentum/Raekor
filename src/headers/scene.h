#pragma once

#include "ecs.h"
#include "components.h"
#include "rmath.h"
#include "assets.h"

namespace Raekor
{

class Scene {
public:
	Scene();
	~Scene();
	Scene(const Scene& rhs) = delete;

	// object management
	entt::entity createObject(const std::string& name = "Empty");
	entt::entity pickObject(Math::Ray& ray);

	// per frame systems
	void updateNode(entt::entity node, entt::entity parent);
	void updateTransforms();
	void loadMaterialTextures(const std::vector<entt::entity>& materials, AssetManager& assetManager);

	// save to disk
	void saveToFile(const std::string& file);
	void openFromFile(const std::string& file, AssetManager& assetManager);

	template<typename T>
	T& Get(entt::entity entity) {
		return registry.get<T>(entity);
	}

	template<typename T>
	T& Add(entt::entity entity) {
		return registry.emplace<T>(entity);
	}

	entt::entity Create() {
		return registry.create();
	}

	// get access to the underlying registry using these
	inline operator entt::registry& () { return registry; }
	inline entt::registry* const operator->() { return &registry; }



private:
	entt::registry registry;
};

} // Namespace Raekor