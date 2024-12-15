#include "pch.h"
#include "Maths.h"
#include "Camera.h"
#include "Member.h"

namespace RK {

RTTI_DEFINE_TYPE(BBox3D)
{
	RTTI_DEFINE_MEMBER(BBox3D, SERIALIZE_ALL, "Min", mMin);
	RTTI_DEFINE_MEMBER(BBox3D, SERIALIZE_ALL, "Max", mMax);
}

static std::default_random_engine sDefaultRandomEngine;
static std::uniform_real_distribution<float> sUniformDistributionZO(0.0, 1.0);
static std::uniform_real_distribution<float> sUniformDistributionNO(-1.0, 1.0);
static std::uniform_int_distribution<> sUniformIntDistribution(INT_MIN, INT_MAX);
static std::uniform_int_distribution<uint32_t> sUniformUIntDistribution(0, UINT32_MAX);



BBox3D& BBox3D::Scale(const Vec3& inScale)
{
	mMin *= inScale;
	mMax *= inScale;
	return *this;
}


BBox3D& BBox3D::Combine(const BBox3D& inOther)
{
	mMin = glm::min(mMin, inOther.mMin);
	mMax = glm::max(mMax, inOther.mMax);
	return *this;
}


BBox3D& BBox3D::Transform(const Mat4x4& inTransform)
{
	mMin = inTransform * Vec4(mMin, 1.0);
	mMax = inTransform * Vec4(mMax, 1.0);
	return *this;
}



Ray::Ray(const Viewport& inViewport, Vec2 inCoords)
{
	Vec3 ndc_to_clip = Vec3 {
	   ( inCoords.x / inViewport.GetRenderSize().x ) * 2.0f - 1.0f,
	   ( inCoords.y / inViewport.GetRenderSize().y ) * 2.0f - 1.0f,
		1.0f
	};

	Vec4 clip_ray = Vec4 { ndc_to_clip.x, ndc_to_clip.y, -1.0f, 1.0f };

	Vec4 view_ray = glm::inverse(inViewport.GetProjection()) * clip_ray;
	view_ray.z = -1.0f, view_ray.w = 0.0f;

	Vec4 world_ray = glm::inverse(inViewport.GetView()) * view_ray;
	world_ray = glm::normalize(world_ray);

	m_Direction = world_ray;
	m_Origin = inViewport.GetPosition();
}


Ray::Ray(const glm::vec3& inOrigin, const glm::vec3& inDirection) :
	m_Origin(inOrigin),
	m_Direction(inDirection),
	m_RcpDirection(1.0f / inDirection)
{
}


std::optional<float> Ray::HitsOBB(const BBox3D& inOBB, const Mat4x4& inTransform) const
{
	float t_min = 0.0f;
	float t_max = 100000.0f;

	const Vec3 obb_pos_ws = Vec3(inTransform[3]);
	const Vec3 delta = obb_pos_ws - m_Origin;

	{
		const Vec3 xaxis = Vec3(inTransform[0]);
		const float e = glm::dot(xaxis, delta);
		const float f = glm::dot(m_Direction, xaxis);

		if (fabs(f) > 0.001f)
		{
			float t1 = ( e + inOBB.mMin.x ) / f;
			float t2 = ( e + inOBB.mMax.x ) / f;

			if (t1 > t2) std::swap(t1, t2);

			if (t2 < t_max) t_max = t2;
			if (t1 > t_min) t_min = t1;
			if (t_min > t_max) return std::nullopt;

		}
		else
		{
			//if (-e + inMin.x > 0.0f || -e + inMax.x < 0.0f) return std::nullopt;
		}
	}


	{
		Vec3 yaxis(inTransform[1]);
		float e = glm::dot(yaxis, delta);
		float f = glm::dot(m_Direction, yaxis);

		if (fabs(f) > 0.001f)
		{

			float t1 = ( e + inOBB.mMin.y ) / f;
			float t2 = ( e + inOBB.mMax.y ) / f;

			if (t1 > t2) std::swap(t1, t2);

			if (t2 < t_max) t_max = t2;
			if (t1 > t_min) t_min = t1;
			if (t_min > t_max) return std::nullopt;

		}
		else
		{
			//if (-e + inMin.y > 0.0f || -e + inMax.y < 0.0f) return std::nullopt;
		}
	}


	{
		Vec3 zaxis(inTransform[2]);
		float e = glm::dot(zaxis, delta);
		float f = glm::dot(m_Direction, zaxis);

		if (fabs(f) > 0.001f)
		{

			float t1 = ( e + inOBB.mMin.z ) / f;
			float t2 = ( e + inOBB.mMax.z ) / f;

			if (t1 > t2) std::swap(t1, t2);

			if (t2 < t_max) t_max = t2;
			if (t1 > t_min) t_min = t1;
			if (t_min > t_max) return std::nullopt;

		}
		else
		{
			//if (-e + inMin.z > 0.0f || -e + inMax.z < 0.0f) return std::nullopt;
		}
	}

	return t_min;
}


std::optional<float> Ray::HitsAABB(const BBox3D& inAABB) const
{
	float t1 = ( inAABB.mMin.x - m_Origin.x ) * m_RcpDirection.x;
	float t2 = ( inAABB.mMax.x - m_Origin.x ) * m_RcpDirection.x;

	float tmin = glm::min(t1, t2);
	float tmax = glm::max(t1, t2);

	for (int i = 1; i < 3; i++)
	{
		t1 = ( inAABB.mMin[i] - m_Origin[i] ) * m_RcpDirection[i];
		t1 = ( inAABB.mMax[i] - m_Origin[i] ) * m_RcpDirection[i];

		tmin = glm::max(tmin, glm::min(glm::min(t1, t2), tmax));
		tmax = glm::min(tmax, glm::max(glm::max(t1, t2), tmin));
	}

	return tmax > glm::max(tmin, 0.0f) ? std::make_optional(tmax) : std::nullopt;
}


std::optional<float> Ray::HitsTriangle(const Vec3& inV0, const Vec3& inV1, const Vec3& inV2, Vec2& outBarycentrics) const
{
	float dist;
	bool hit = glm::intersectRayTriangle(m_Origin, m_Direction, inV0, inV1, inV2, outBarycentrics, dist);

	return hit ? std::make_optional(dist) : std::nullopt;
}


std::optional<float> Ray::HitsSphere(const Vec3& inPosition, float inRadius, float inTmin, float inTmax) const
{
	float dist = FLT_MAX;
	bool hit = glm::intersectRaySphere(m_Origin, m_Direction, inPosition, inRadius * inRadius, dist);

	return hit ? std::make_optional(dist) : std::nullopt;
}


bool BBox3D::Contains(const Vec3& inPoint) const
{
	return ( inPoint.x >= mMin.x && inPoint.x <= mMax.x ) &&
		( inPoint.y >= mMin.y && inPoint.y <= mMax.y ) &&
		( inPoint.z >= mMin.z && inPoint.z <= mMax.z );
}

int gRandomInt()
{
	return sUniformIntDistribution(sDefaultRandomEngine);
}

uint32_t gRandomUInt()
{
	return sUniformUIntDistribution(sDefaultRandomEngine);
}

float gRandomFloatZO()
{
	return sUniformDistributionZO(sDefaultRandomEngine);
}

float gRandomFloatNO()
{
	return sUniformDistributionNO(sDefaultRandomEngine);
}

Mat3x3 gRandomOrientation()
{
	return glm::mat3_cast(
		glm::angleAxis(gRandomFloatZO() * ( float(M_PI) * 2.0f ),
			glm::normalize(glm::vec3(gRandomFloatNO(), gRandomFloatNO(), gRandomFloatNO()))));
}


/* "Fast Random Rotation Matrices" - James Arvo, Graphics Gems 3 */
Mat3x3 gRandomRotationMatrix()
{
	float x[3] = { gRandomFloatZO(), gRandomFloatZO(), gRandomFloatZO() };

	constexpr float PITIMES2 = M_PI * 2;
	float theta = x[0] * PITIMES2;  /* Rotation about the pole (Z).      */
	float phi = x[1] * PITIMES2;    /* For direction of pole deflection. */
	float z = x[2] * 2.f;           /* For magnitude of pole deflection. */

	/* Compute a vector V used for distributing points over the sphere  */
	/* via the reflection I - V Transpose(V).  This formulation of V    */
	/* will guarantee that if x[1] and x[2] are uniformly distributed,  */
	/* the reflected points will be uniform on the sphere.  Note that V */
	/* has length sqrt(2) to eliminate the 2 in the Householder matrix. */

	float r = sqrtf(z);
	float Vx = sinf(phi) * r;
	float Vy = cosf(phi) * r;
	float Vz = sqrtf(2.f - z);

	/* Compute the row vector S = Transpose(V) * R, where R is a simple */
	/* rotation by theta about the z-axis.  No need to compute Sz since */
	/* it's just Vz.                                                    */

	float st = sinf(theta);
	float ct = cosf(theta);
	float Sx = Vx * ct - Vy * st;
	float Sy = Vx * st + Vy * ct;

	/* Construct the rotation matrix  ( V Transpose(V) - I ) R, which   */
	/* is equivalent to V S - R.                                        */

	Mat3x3 matrix = Mat3x3(1.0f);

	matrix[0][0] = Vx * Sx - ct;
	matrix[0][1] = Vx * Sy - st;
	matrix[0][2] = Vx * Vz;

	matrix[1][0] = Vy * Sx + st;
	matrix[1][1] = Vy * Sy - ct;
	matrix[1][2] = Vy * Vz;

	matrix[2][0] = Vz * Sx;
	matrix[2][1] = Vz * Sy;
	matrix[2][2] = 1.0 - z;   /* This equals Vz * Vz - 1.0 */

	return glm::transpose(matrix);
}


/*
	Fast Extraction of Viewing Frustum Planes from the World-View-Projection Matrix by G. Gribb & K. Hartmann
	https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
*/
Frustum::Frustum(const glm::mat4& inViewProjMatrix, bool inShouldNormalize)
{
	m_Planes[0].x = inViewProjMatrix[0][3] + inViewProjMatrix[0][0];
	m_Planes[0].y = inViewProjMatrix[1][3] + inViewProjMatrix[1][0];
	m_Planes[0].z = inViewProjMatrix[2][3] + inViewProjMatrix[2][0];
	m_Planes[0].w = inViewProjMatrix[3][3] + inViewProjMatrix[3][0];

	m_Planes[1].x = inViewProjMatrix[0][3] - inViewProjMatrix[0][0];
	m_Planes[1].y = inViewProjMatrix[1][3] - inViewProjMatrix[1][0];
	m_Planes[1].z = inViewProjMatrix[2][3] - inViewProjMatrix[2][0];
	m_Planes[1].w = inViewProjMatrix[3][3] - inViewProjMatrix[3][0];

	m_Planes[2].x = inViewProjMatrix[0][3] - inViewProjMatrix[0][1];
	m_Planes[2].y = inViewProjMatrix[1][3] - inViewProjMatrix[1][1];
	m_Planes[2].z = inViewProjMatrix[2][3] - inViewProjMatrix[2][1];
	m_Planes[2].w = inViewProjMatrix[3][3] - inViewProjMatrix[3][1];

	m_Planes[3].x = inViewProjMatrix[0][3] + inViewProjMatrix[0][1];
	m_Planes[3].y = inViewProjMatrix[1][3] + inViewProjMatrix[1][1];
	m_Planes[3].z = inViewProjMatrix[2][3] + inViewProjMatrix[2][1];
	m_Planes[3].w = inViewProjMatrix[3][3] + inViewProjMatrix[3][1];

	m_Planes[4].x = inViewProjMatrix[0][3] + inViewProjMatrix[0][2];
	m_Planes[4].y = inViewProjMatrix[1][3] + inViewProjMatrix[1][2];
	m_Planes[4].z = inViewProjMatrix[2][3] + inViewProjMatrix[2][2];
	m_Planes[4].w = inViewProjMatrix[3][3] + inViewProjMatrix[3][2];

	m_Planes[5].x = inViewProjMatrix[0][3] - inViewProjMatrix[0][2];
	m_Planes[5].y = inViewProjMatrix[1][3] - inViewProjMatrix[1][2];
	m_Planes[5].z = inViewProjMatrix[2][3] - inViewProjMatrix[2][2];
	m_Planes[5].w = inViewProjMatrix[3][3] - inViewProjMatrix[3][2];

	if (inShouldNormalize)
	{
		for (Vec4& plane : m_Planes)
		{
			const float mag = sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
			plane.x = plane.x / mag;
			plane.y = plane.y / mag;
			plane.z = plane.z / mag;
			plane.w = plane.w / mag;
		}
	}
}



bool Frustum::Contains(const Vec3& inPoint) const
{
	for (const Vec4& plane : m_Planes)
	{
		if (glm::dot(plane, glm::vec4(inPoint, 1.0f)) < 0.0)
			return false;
	}

	return true;
}



bool Frustum::ContainsAABB(const BBox3D& inAABB) const
{
	for (const Vec4& plane : m_Planes)
	{
		int out = 0;
		out += ( ( glm::dot(plane, glm::vec4(inAABB.mMin.x, inAABB.mMin.y, inAABB.mMin.z, 1.0f)) < 0.0 ) ? 1 : 0 );
		out += ( ( glm::dot(plane, glm::vec4(inAABB.mMax.x, inAABB.mMin.y, inAABB.mMin.z, 1.0f)) < 0.0 ) ? 1 : 0 );
		out += ( ( glm::dot(plane, glm::vec4(inAABB.mMin.x, inAABB.mMax.y, inAABB.mMin.z, 1.0f)) < 0.0 ) ? 1 : 0 );
		out += ( ( glm::dot(plane, glm::vec4(inAABB.mMax.x, inAABB.mMax.y, inAABB.mMin.z, 1.0f)) < 0.0 ) ? 1 : 0 );
		out += ( ( glm::dot(plane, glm::vec4(inAABB.mMin.x, inAABB.mMin.y, inAABB.mMax.z, 1.0f)) < 0.0 ) ? 1 : 0 );
		out += ( ( glm::dot(plane, glm::vec4(inAABB.mMax.x, inAABB.mMin.y, inAABB.mMax.z, 1.0f)) < 0.0 ) ? 1 : 0 );
		out += ( ( glm::dot(plane, glm::vec4(inAABB.mMin.x, inAABB.mMax.y, inAABB.mMax.z, 1.0f)) < 0.0 ) ? 1 : 0 );
		out += ( ( glm::dot(plane, glm::vec4(inAABB.mMax.x, inAABB.mMax.y, inAABB.mMax.z, 1.0f)) < 0.0 ) ? 1 : 0 );

		if (out == 8)
			return false;
	}

	return true;
}

} // raekor

RTTI_DEFINE_TYPE_PRIMITIVE(RK::Vec2);
RTTI_DEFINE_TYPE_PRIMITIVE(RK::Vec3);
RTTI_DEFINE_TYPE_PRIMITIVE(RK::Vec4);
RTTI_DEFINE_TYPE_PRIMITIVE(RK::Mat4x4);