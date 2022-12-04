#pragma once

namespace Raekor {

class KeyFrames {
	friend class GltfImporter;
	friend class AssimpImporter;

public:
	KeyFrames() = default;
	KeyFrames(uint32_t inBoneIndex) : m_BoneIndex(inBoneIndex) {}

	void LoadFromAssimp(const aiNodeAnim* nodeAnim);
	void LoadFromGltf(const cgltf_animation* nodeAnim);

	glm::vec3 GetInterpolatedScale(float animationTime) const;
	glm::quat GetInterpolatedRotation(float animationTime) const;
	glm::vec3 GetInterpolatedPosition(float animationTime) const;

private:
	uint32_t m_BoneIndex;
	std::vector<aiVectorKey> positionKeys;
	std::vector<aiQuatKey> rotationkeys;
	std::vector<aiVectorKey> scaleKeys;
};


class Animation {
	friend struct Skeleton;
	friend class GltfImporter;
	friend class AssimpImporter;

public:
	Animation(const aiAnimation* inAnimation);
	Animation(const cgltf_animation* inAnimation);

	const std::string& GetName() const { return m_Name; }
	inline float GetRunningTime() const { return m_RunningTime; }
	inline float GetTotalDuration() const { return m_TotalDuration; }

	void LoadKeyframes(uint32_t inBoneIndex, const aiNodeAnim* inAnimation);
	void LoadKeyframes(uint32_t inBoneIndex, const cgltf_animation* inAnimation);

private:
	/* Elapsed time in milliseconds. */
	float m_RunningTime = 0.0f;
	/* Total duration of the animation.*/
	float m_TotalDuration = 0.0f;
	/* Name of the animation. */
	std::string m_Name;
	/* Arrays of keyframes mapped to bone/joint indices. */
	std::unordered_map<uint32_t, KeyFrames> m_BoneAnimations;
};

} // raekor
