#include "pch.h"
#include "anim.h"

namespace Raekor {

void BoneAnimation::loadFromAssimp(aiNodeAnim* nodeAnim) {
	for (unsigned int i = 0; i < nodeAnim->mNumScalingKeys; i++) {
		scaleKeys.push_back(nodeAnim->mScalingKeys[i]);
	}

	for (unsigned int i = 0; i < nodeAnim->mNumRotationKeys; i++) {
		rotationkeys.push_back(nodeAnim->mRotationKeys[i]);
	}

	for (unsigned int i = 0; i < nodeAnim->mNumPositionKeys; i++) {
		positionKeys.push_back(nodeAnim->mPositionKeys[i]);
	}
}



glm::vec3 BoneAnimation::getInterpolatedPosition(float animationTime) const {
	if (positionKeys.size() == 1) {
		// No interpolation necessary for single value
		auto v = positionKeys[0].mValue;
		return { v.x, v.y, v.z };
	}

	uint32_t PositionIndex = 0;
	for (unsigned int i = 0; i < positionKeys.size() - 1; i++) {
		if (animationTime < (float)positionKeys[i + 1].mTime) {
			PositionIndex = i;
			break;
		}
	}

	uint32_t NextPositionIndex = (PositionIndex + 1);

	float DeltaTime = (float)(positionKeys[NextPositionIndex].mTime - positionKeys[PositionIndex].mTime);
	float Factor = (animationTime - (float)positionKeys[PositionIndex].mTime) / DeltaTime;
	if (Factor < 0.0f) Factor = 0.0f;

	const aiVector3D& Start = positionKeys[PositionIndex].mValue;
	const aiVector3D& End = positionKeys[NextPositionIndex].mValue;
	aiVector3D Delta = End - Start;
	auto aiVec = Start + Factor * Delta;
	return { aiVec.x, aiVec.y, aiVec.z };
}
	


glm::quat BoneAnimation::getInterpolatedRotation(float animationTime) const {
	if (rotationkeys.size() == 1) {
		// No interpolation necessary for single value
		auto v = rotationkeys[0].mValue;
		return glm::quat(v.w, v.x, v.y, v.z);
	}

	uint32_t RotationIndex = 0;
	for (unsigned int i = 0; i < rotationkeys.size() - 1; i++) {
		if (animationTime < (float)rotationkeys[i + 1].mTime) {
			RotationIndex = i;
			break;
		}

	}
	uint32_t NextRotationIndex = (RotationIndex + 1);

	float DeltaTime = (float)(rotationkeys[NextRotationIndex].mTime - rotationkeys[RotationIndex].mTime);
	float Factor = (animationTime - (float)rotationkeys[RotationIndex].mTime) / DeltaTime;
	if (Factor < 0.0f) Factor = 0.0f;

	const aiQuaternion& StartRotationQ = rotationkeys[RotationIndex].mValue;
	const aiQuaternion& EndRotationQ = rotationkeys[NextRotationIndex].mValue;
	auto q = aiQuaternion();
	aiQuaternion::Interpolate(q, StartRotationQ, EndRotationQ, Factor);
	q = q.Normalize();
	return glm::quat(q.w, q.x, q.y, q.z);
}



glm::vec3 BoneAnimation::getInterpolatedScale(float animationTime) const {
	if (scaleKeys.size() == 1) {
		// No interpolation necessary for single value
		auto v = scaleKeys[0].mValue;
		return { v.x, v.y, v.z };
	}

	uint32_t index = 0;
	for (unsigned int i = 0; i < scaleKeys.size() - 1; i++) {
		if (animationTime < (float)scaleKeys[i + 1].mTime) {
			index = i;
			break;
		}
	}
	uint32_t nextIndex = (index + 1);

	float deltaTime = (float)(scaleKeys[nextIndex].mTime - scaleKeys[index].mTime);
	float factor = (animationTime - (float)scaleKeys[index].mTime) / deltaTime;
	if (factor < 0.0f) factor = 0.0f;

	const auto& start = scaleKeys[index].mValue;
	const auto& end = scaleKeys[nextIndex].mValue;
	auto delta = end - start;
	auto aiVec = start + factor * delta;
	return { aiVec.x, aiVec.y, aiVec.z };
}



Animation::Animation(aiAnimation* anim) {
	loadFromAssimp(anim);
}



void Animation::loadFromAssimp(aiAnimation* anim) {
	name = anim->mName.C_Str();
	ticksPerSecond = static_cast<float>(anim->mTicksPerSecond);
	totalDuration = static_cast<float>(anim->mDuration);
	runningTime = 0;

	for (unsigned int ch = 0; ch < anim->mNumChannels; ch++) {
		auto aiNodeAnim = anim->mChannels[ch];

		boneAnimations[aiNodeAnim->mNodeName.C_Str()] = {};
		auto& nodeAnim = boneAnimations[aiNodeAnim->mNodeName.C_Str()];

		nodeAnim.loadFromAssimp(aiNodeAnim);
	}
}

} // raekor