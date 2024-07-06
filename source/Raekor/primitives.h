#pragma once

namespace RK {

struct Vertex
{
	constexpr Vertex(
		Vec3 p = {},
		Vec2 uv = {},
		Vec3 n = {},
		Vec3 t = {}
	) :
		pos(p),
		uv(uv),
		normal(n),
		tangent(t)
	{
	}

	Vec3 pos;
	Vec2 uv;
	Vec3 normal;
	Vec3 tangent;
};


struct Triangle
{
	constexpr Triangle(
		uint32_t _p1 = {},
		uint32_t _p2 = {},
		uint32_t _p3 = {}
	) :
		p1(_p1),
		p2(_p2),
		p3(_p3)
	{
	}

	uint32_t p1, p2, p3;
};

}