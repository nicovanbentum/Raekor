#pragma once

namespace Raekor {

class BoneAnimation {
public:
	void loadFromAssimp(aiNodeAnim* nodeAnim);

	glm::vec3 getInterpolatedScale(float animationTime);
	glm::quat getInterpolatedRotation(float animationTime);
	glm::vec3 getInterpolatedPosition(float animationTime);

private:
	std::vector<aiVectorKey> positionKeys;
	std::vector<aiQuatKey> rotationkeys;
	std::vector<aiVectorKey> scaleKeys;
};

class Animation {
public:
	Animation() {}
	Animation(aiAnimation* anim);

	void loadFromAssimp(aiAnimation* anim);

	std::string name;
	float ticksPerSecond;
	float totalDuration;
	float runningTime;
	std::unordered_map<std::string, BoneAnimation> boneAnimations;
};

} // raekor