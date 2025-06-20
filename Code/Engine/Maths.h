#pragma once

#include "RTTI.h"

namespace RK {

class Viewport;

template<glm::length_t L, typename T>
std::string gToString(const glm::vec<L, T>& inValue);

template<glm::length_t C, glm::length_t R, typename T>
std::string gToString(const glm::mat<C, R, T>& inValue);

template<glm::length_t L, typename T>
inline glm::vec < L, T> gFromString(const std::string& inValue);

template<glm::length_t C, glm::length_t R, typename T>
inline glm::mat<C, R, T> gFromString(const std::string& inValue);


inline float gRadiansToDegrees(float inValue) { return inValue * ( 180.0f / M_PI ); }
inline float gDegreesToRadians(float inValue) { return inValue * ( M_PI / 180.0f ); }

constexpr bool gIsPow2(size_t value) noexcept { return ( value & ( value - 1 ) ) == 0 && value != 0; }
constexpr inline size_t gAlignUp(size_t value, size_t alignment) noexcept { return ( value + (alignment - 1) ) & ~( alignment - 1 ); }
constexpr inline size_t gAlignDown(size_t value, size_t alignment) noexcept { return value & ~( alignment - 1 ); }


inline uint8_t gUnswizzle(uint8_t swizzle, uint8_t channel) { return ( ( swizzle >> ( channel * 2 ) ) & 0b00'00'00'11 ); }
inline std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> gUnswizzle(uint8_t swizzle) 
{
	return std::make_tuple(gUnswizzle(swizzle, 0), gUnswizzle(swizzle, 1), gUnswizzle(swizzle, 2), gUnswizzle(swizzle, 3));
}


struct BBox3D
{
	RTTI_DECLARE_TYPE(BBox3D);

	BBox3D() = default;
	BBox3D(const Vec3& inMin, const Vec3& inMax) : mMin(inMin), mMax(inMax) {}

	const Vec3& GetMin() const { return mMin; }
	const Vec3& GetMax() const { return mMax; }

	BBox3D& Scale(const Vec3& inScale);
	BBox3D& Combine(const BBox3D& inOther);
	BBox3D& Transform(const Mat4x4& inTransform);

	BBox3D Scaled(const Vec3& inScale) const { return BBox3D(*this).Scale(inScale); }
	BBox3D Combined(const BBox3D& inOther) const { return BBox3D(*this).Combine(inOther); }
	BBox3D Transformed(const Mat4x4& inTransform) const { return BBox3D(*this).Transform(inTransform); }

	bool Contains(const Vec3& inPoint) const;

	Vec3 GetCenter() const { return mMin + ( ( mMax - mMin ) * 0.5f ); }
	Vec3 GetExtents() const { return glm::abs(mMax - mMin); }

	float GetDepth() const { return GetExtents().z; }
	float GetWidth() const { return GetExtents().x; }
	float GetHeight() const { return GetExtents().y; }

	bool IsValid() const { return glm::all(glm::greaterThan(mMax, mMin)); }

	Vec3 mMin = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
	Vec3 mMax = Vec3(FLT_MIN, FLT_MIN, FLT_MIN);
};


struct Ray
{
	Ray(const Viewport& viewport, Vec2 coords);
	Ray(const Vec3& origin, const Vec3& direction);

	Vec3 m_Origin;
	Vec3 m_Direction;
	Vec3 m_RcpDirection;

	Optional<float> HitsOBB(const BBox3D& inOBB, const Mat4x4& modelMatrix) const;
	Optional<float> HitsAABB(const BBox3D& inABB) const;
	Optional<float> HitsTriangle(const Vec3& inV0, const Vec3& inV1, const Vec3& inV2, Vec2& outBarycentrics) const;
	Optional<float> HitsSphere(const Vec3& inPosition, float inRadius, float inTmin, float inTmax) const;
};


struct Frustum
{
	enum Halfspace
	{
		NEGATIVE = -1,
		ON_PLANE = 0,
		POSITIVE = 1
	};

	StaticArray<Vec4, 6> m_Planes;

	Frustum() = default;
	Frustum(const Mat4x4& inViewProjMatrix, bool inNormalize);

	bool Contains(const Vec3& inPoint) const;
	bool ContainsAABB(const BBox3D& inAABB) const;
};


int gRandomInt();
uint32_t gRandomUInt();
float gRandomFloatZO();
float gRandomFloatNO();
Mat3x3 gRandomOrientation();
Mat3x3 gRandomRotationMatrix();


template<glm::length_t L, typename T>
inline std::string gToString(const glm::vec<L, T>& inValue)
{
	std::stringstream ss;
	ss << "(";
	for (int i = 0; i < L; i++)
	{
		ss << inValue[i];
		if (i != L - 1) ss << " ";
	}
	ss << ")";
	return ss.str();
}


template<glm::length_t C, glm::length_t R, typename T>
inline std::string gToString(const glm::mat<C, R, T>& inValue)
{
	std::stringstream ss;
	ss << "((";
	for (int i = 0; i < C; ++i)
	{
		for (int j = 0; j < R; ++j)
		{
			ss << inValue[i][j];
			if (j != R - 1) ss << " ";
		}
		if (i != C - 1) ss << ") (";
	}
	ss << "))";

	return ss.str();
}


inline std::string gToString(const glm::quat& inValue)
{
	std::stringstream ss;
	ss << "(";
	for (int i = 0; i < glm::quat::length(); i++)
	{
		ss << inValue[i];
		if (i != glm::quat::length() - 1) ss << " ";
	}
	ss << ")";
	return ss.str();
}



template<glm::length_t L, typename T>
inline glm::vec < L, T> gFromString(const std::string& inValue)
{
	glm::vec < L, T> result;
	std::stringstream ss(inValue);

	char delim;
	ss >> delim;

	for (int i = 0; i < L; i++)
		ss >> result[i];

	return result;
}

template<glm::length_t C, glm::length_t R, typename T>
inline glm::mat<C, R, T> gFromString(const std::string& inValue)
{
	glm::mat<C, R, T> result;
	std::stringstream ss(inValue);

	char delim;
	ss >> delim >> delim; // eat ((

	for (int i = 0; i < C; ++i)
	{
		for (int j = 0; j < R; ++j)
		{
			ss >> result[i][j];
		}
		ss >> delim >> delim; // eat )( and ))
	}

	return result;
}


inline glm::quat gFromString(const std::string& inValue)
{
	glm::quat result;
	std::stringstream ss(inValue);

	char delim;
	ss >> delim;

	for (int i = 0; i < glm::quat::length(); i++)
		ss >> result[i];

	return result;
}

} // namespace Raekor

RTTI_DECLARE_TYPE_PRIMITIVE(RK::Vec2);
RTTI_DECLARE_TYPE_PRIMITIVE(RK::Vec3);
RTTI_DECLARE_TYPE_PRIMITIVE(RK::Vec4);
RTTI_DECLARE_TYPE_PRIMITIVE(RK::Mat4x4);