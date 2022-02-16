#pragma once

namespace Raekor {

class BoneAnimation {
public:
	void loadFromAssimp(aiNodeAnim* nodeAnim);

	glm::vec3 getInterpolatedScale(float animationTime) const;
	glm::quat getInterpolatedRotation(float animationTime) const;
	glm::vec3 getInterpolatedPosition(float animationTime) const;

private:
	std::vector<aiVectorKey> positionKeys;
	std::vector<aiQuatKey> rotationkeys;
	std::vector<aiVectorKey> scaleKeys;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class Animation {
	friend struct Skeleton;;

public:
	Animation() = default;
	Animation(aiAnimation* anim);

	std::string& GetName() { return name; }
	inline float getCurrentTime() { return runningTime; }
	inline float getTotalDuration() { return totalDuration; }

	void loadFromAssimp(aiAnimation* anim);

private:
	std::string name;
	float ticksPerSecond;
	float totalDuration;
	float runningTime;
	std::unordered_map<std::string, BoneAnimation> boneAnimations;
};

} // raekor