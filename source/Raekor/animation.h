#pragma once

namespace Raekor {

class KeyFrames {
public:
	void LoadFromAssimp(aiNodeAnim* nodeAnim);

	glm::vec3 GetInterpolatedScale(float animationTime) const;
	glm::quat GetInterpolatedRotation(float animationTime) const;
	glm::vec3 GetInterpolatedPosition(float animationTime) const;

private:
	std::vector<aiVectorKey> scaleKeys;
	std::vector<aiQuatKey> rotationkeys;
	std::vector<aiVectorKey> positionKeys;
};


class Animation {
	friend struct Skeleton;

public:
	Animation() = default;
	Animation(aiAnimation* anim);

	const std::string& GetName() const { return m_Name; }
	inline float GetRunningTime() const { return m_RunningTime; }
	inline float GetTotalDuration() const { return m_TotalDuration; }

	void LoadFromAssimp(aiAnimation* anim);

private:
	float m_RunningTime;
	float m_TotalDuration;
	float m_TicksPerSecond;

	std::string m_Name;
	std::unordered_map<std::string, KeyFrames> m_BoneAnimations;
};

} // raekor
