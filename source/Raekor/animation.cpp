#include "pch.h"
#include "animation.h"
#include "util.h"

namespace Raekor {

void KeyFrames::LoadFromAssimp(const aiNodeAnim* nodeAnim) {
	for (const auto& key : Slice(nodeAnim->mScalingKeys, nodeAnim->mNumScalingKeys))
		scaleKeys.push_back(key);

	for (const auto& key : Slice(nodeAnim->mPositionKeys, nodeAnim->mNumPositionKeys))
		positionKeys.push_back(key);

	for (const auto& key : Slice(nodeAnim->mRotationKeys, nodeAnim->mNumRotationKeys))
		rotationkeys.push_back(key);
}


void KeyFrames::LoadFromGltf(const cgltf_animation* nodeAnim) {

}


glm::vec3 KeyFrames::GetInterpolatedPosition(float animationTime) const {
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
	

glm::quat KeyFrames::GetInterpolatedRotation(float animationTime) const {
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
	if (Factor < 0.0f) 
		Factor = 0.0f;

	const aiQuaternion& StartRotationQ = rotationkeys[RotationIndex].mValue;
	const aiQuaternion& EndRotationQ = rotationkeys[NextRotationIndex].mValue;
	auto q = aiQuaternion();
	aiQuaternion::Interpolate(q, StartRotationQ, EndRotationQ, Factor);
	q = q.Normalize();
	return glm::quat(q.w, q.x, q.y, q.z);
}


glm::vec3 KeyFrames::GetInterpolatedScale(float animationTime) const {
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

Animation::Animation(const aiAnimation* inAnimation) {
	m_Name = inAnimation->mName.C_Str();
	m_RunningTime = 0;
	m_TotalDuration = float(inAnimation->mDuration);
}

Animation::Animation(const cgltf_animation* inAnimation) {
	m_Name = inAnimation->name;
	m_RunningTime = 0;
	m_TotalDuration = 0;
}


void Animation::LoadKeyframes(uint32_t inBoneIndex, const aiNodeAnim* inAnimation) {
	m_BoneAnimations[inBoneIndex] = KeyFrames(inBoneIndex);
	m_BoneAnimations[inBoneIndex].LoadFromAssimp(inAnimation);
}


void Animation::LoadKeyframes(uint32_t inBoneIndex, const cgltf_animation* inAnimation) {
	m_BoneAnimations[inBoneIndex] = KeyFrames(inBoneIndex);
	m_BoneAnimations[inBoneIndex].LoadFromGltf(inAnimation);
}


} // raekor
