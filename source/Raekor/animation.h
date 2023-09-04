#pragma once

#include "rtti.h"

namespace Raekor {

template<typename T>
struct Key
{
	double mTime = 0.0f;
	T mValue = {};

	Key() = default;
	Key(double inTime, const T& inValue) : mTime(inTime), mValue(inValue) {}

	inline bool operator==(const T& inOther) const { return inOther.mValue == mValue; }
	inline bool operator!=(const T& inOther) const { return inOther.mValue != mValue; }

	inline bool operator<(const T& inOther) const { return mTime < inOther.mTime; }
	inline bool operator>(const T& inOther) const { return mTime > inOther.mTime; }
};

struct Vec3Key : public Key<Vec3>
{
	RTTI_DECLARE_TYPE(Vec3Key);
};

struct QuatKey : public Key<Quat>
{
	RTTI_DECLARE_TYPE(QuatKey);
};

class KeyFrames
{
	RTTI_DECLARE_TYPE(KeyFrames);

	friend class GltfImporter;
	friend class AssimpImporter;

	template<typename T>
	struct Key
	{
		double mTime = 0.0f;
		T mValue = {};

		Key() = default;
		Key(double inTime, const T& inValue) : mTime(inTime), mValue(inValue) {}

		inline bool operator==(const T& inOther) const { return inOther.mValue == mValue; }
		inline bool operator!=(const T& inOther) const { return inOther.mValue != mValue; }

		inline bool operator<(const T& inOther) const { return mTime < inOther.mTime; }
		inline bool operator>(const T& inOther) const { return mTime > inOther.mTime; }
	};


public:
	KeyFrames() = default;
	KeyFrames(uint32_t inBoneIndex) : m_BoneIndex(inBoneIndex) {}

#ifndef DEPRECATE_ASSIMP
	void LoadFromAssimp(const aiNodeAnim* inNodeAnim);
#endif
	void LoadFromGltf(const cgltf_animation* inNodeAnim);

	Vec3 GetInterpolatedScale(float animationTime) const;
	Quat GetInterpolatedRotation(float animationTime) const;
	Vec3 GetInterpolatedPosition(float animationTime) const;

private:
	uint32_t			 m_BoneIndex;
	std::vector<Vec3Key> m_ScaleKeys;
	std::vector<Vec3Key> m_PositionKeys;
	std::vector<QuatKey> m_RotationKeys;
};


class Animation
{
	RTTI_DECLARE_TYPE(Animation);

	friend struct Skeleton;
	friend class GltfImporter;
	friend class AssimpImporter;

public:
	Animation() = default;
#ifndef DEPRECATE_ASSIMP
	Animation(const aiAnimation* inAnimation);
#endif
	Animation(const cgltf_animation* inAnimation);

	const std::string& GetName() const { return m_Name; }
	inline float GetRunningTime() const { return m_RunningTime; }
	inline float GetTotalDuration() const { return m_TotalDuration; }

#ifndef DEPRECATE_ASSIMP
	void LoadKeyframes(uint32_t inBoneIndex, const aiNodeAnim* inAnimation);
#endif
	void LoadKeyframes(uint32_t inBoneIndex, const cgltf_animation* inAnimation);

private:
	/* Elapsed time in milliseconds. */
	float m_RunningTime = 0.0f;
	/* Name of the animation. */
	std::string m_Name;
	/* Total duration of the animation.*/
	float m_TotalDuration = 0.0f;
	/* Arrays of keyframes mapped to bone/joint indices. */
	std::unordered_map<uint32_t, KeyFrames> m_BoneAnimations;
};

} // raekor
