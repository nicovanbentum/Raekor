#pragma once

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


constexpr inline size_t gAlignUp(size_t value, size_t alignment) noexcept { return ( value + alignment - 1 ) & ~( alignment - 1 ); }
constexpr inline size_t gAlignDown(size_t value, size_t alignment) noexcept { return value & ~( alignment - 1 ); }


inline uint8_t gUnswizzle(uint8_t swizzle, uint8_t channel) { return ( ( swizzle >> ( channel * 2 ) ) & 0b00'00'00'11 ); }
inline std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> gUnswizzle(uint8_t swizzle) 
{
	return std::make_tuple(gUnswizzle(swizzle, 0), gUnswizzle(swizzle, 1), gUnswizzle(swizzle, 2), gUnswizzle(swizzle, 3));
}


struct BBox3D
{
	BBox3D() = default;
	BBox3D(const Vec3& inMin, const Vec3& inMax) : mMin(inMin), mMax(inMax) {}

	const Vec3& GetMin() const { return mMin; }
	const Vec3& GetMax() const { return mMax; }

	BBox3D& Scale(const Vec3& inScale);
	BBox3D& Combine(const BBox3D& inOther);
	BBox3D& Transform(const Mat4x4& inTransform);

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

	std::optional<float> HitsOBB(const BBox3D& inOBB, const Mat4x4& modelMatrix) const;
	std::optional<float> HitsAABB(const BBox3D& inABB) const;
	std::optional<float> HitsTriangle(const Vec3& inV0, const Vec3& inV1, const Vec3& inV2, Vec2& outBarycentrics) const;
	std::optional<float> HitsSphere(const Vec3& inPosition, float inRadius, float inTmin, float inTmax) const;
};


struct Frustum
{
	enum Halfspace
	{
		NEGATIVE = -1,
		ON_PLANE = 0,
		POSITIVE = 1
	};

	std::array<Vec4, 6> m_Planes;

	Frustum() = default;
	Frustum(const glm::mat4& inViewProjMatrix, bool inShouldNormalize);

	bool Contains(const Vec3& inPoint) const;
	bool ContainsAABB(const BBox3D& inAABB) const;
};



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