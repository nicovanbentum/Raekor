#include "pch.h"
#include "animation.h"
#include "util.h"

namespace Raekor {

void KeyFrames::LoadFromAssimp(const aiNodeAnim* nodeAnim) {
	for (const auto& key : Slice(nodeAnim->mScalingKeys, nodeAnim->mNumScalingKeys))
		m_ScaleKeys.push_back(key);

	for (const auto& key : Slice(nodeAnim->mPositionKeys, nodeAnim->mNumPositionKeys))
		m_PositionKeys.push_back(key);

	for (const auto& key : Slice(nodeAnim->mRotationKeys, nodeAnim->mNumRotationKeys))
		m_RotationKeys.push_back(key);
}


void KeyFrames::LoadFromGltf(const cgltf_animation* nodeAnim) {

}


glm::vec3 KeyFrames::GetInterpolatedPosition(float animationTime) const {
	if (m_PositionKeys.size() == 1) {
		// No interpolation necessary for single value
		auto v = m_PositionKeys[0].mValue;
		return { v.x, v.y, v.z };
	}

	uint32_t PositionIndex = 0;
	for (unsigned int i = 0; i < m_PositionKeys.size() - 1; i++) {
		if (animationTime < (float)m_PositionKeys[i + 1].mTime) {
			PositionIndex = i;
			break;
		}
	}

	uint32_t NextPositionIndex = (PositionIndex + 1);

	float DeltaTime = (float)(m_PositionKeys[NextPositionIndex].mTime - m_PositionKeys[PositionIndex].mTime);
	float Factor = (animationTime - (float)m_PositionKeys[PositionIndex].mTime) / DeltaTime;
	if (Factor < 0.0f) Factor = 0.0f;

	const aiVector3D& Start = m_PositionKeys[PositionIndex].mValue;
	const aiVector3D& End = m_PositionKeys[NextPositionIndex].mValue;
	aiVector3D Delta = End - Start;
	auto aiVec = Start + Factor * Delta;
	return { aiVec.x, aiVec.y, aiVec.z };
}
	

glm::quat KeyFrames::GetInterpolatedRotation(float animationTime) const {
	if (m_RotationKeys.size() == 1) {
		// No interpolation necessary for single value
		auto v = m_RotationKeys[0].mValue;
		return glm::quat(v.w, v.x, v.y, v.z);
	}

	uint32_t RotationIndex = 0;
	for (unsigned int i = 0; i < m_RotationKeys.size() - 1; i++) {
		if (animationTime < (float)m_RotationKeys[i + 1].mTime) {
			RotationIndex = i;
			break;
		}
	}

	uint32_t NextRotationIndex = (RotationIndex + 1);

	float DeltaTime = (float)(m_RotationKeys[NextRotationIndex].mTime - m_RotationKeys[RotationIndex].mTime);
	float Factor = (animationTime - (float)m_RotationKeys[RotationIndex].mTime) / DeltaTime;
	if (Factor < 0.0f) 
		Factor = 0.0f;

	const aiQuaternion& StartRotationQ = m_RotationKeys[RotationIndex].mValue;
	const aiQuaternion& EndRotationQ = m_RotationKeys[NextRotationIndex].mValue;
	auto q = aiQuaternion();
	aiQuaternion::Interpolate(q, StartRotationQ, EndRotationQ, Factor);
	q = q.Normalize();
	return glm::quat(q.w, q.x, q.y, q.z);
}


glm::vec3 KeyFrames::GetInterpolatedScale(float animationTime) const {
	if (m_ScaleKeys.size() == 1) {
		// No interpolation necessary for single value
		auto v = m_ScaleKeys[0].mValue;
		return { v.x, v.y, v.z };
	}

	uint32_t index = 0;
	for (unsigned int i = 0; i < m_ScaleKeys.size() - 1; i++) {
		if (animationTime < (float)m_ScaleKeys[i + 1].mTime) {
			index = i;
			break;
		}
	}

	uint32_t nextIndex = (index + 1);

	float deltaTime = (float)(m_ScaleKeys[nextIndex].mTime - m_ScaleKeys[index].mTime);
	float factor = (animationTime - (float)m_ScaleKeys[index].mTime) / deltaTime;
	if (factor < 0.0f) factor = 0.0f;

	const auto& start = m_ScaleKeys[index].mValue;
	const auto& end = m_ScaleKeys[nextIndex].mValue;
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
