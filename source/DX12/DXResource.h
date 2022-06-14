#pragma once

#include "DXUtil.h"

namespace Raekor::DX {

typedef ComPtr<ID3D12Resource>          ResourceRef;
typedef ComPtr<D3D12MA::Allocation>     AllocationRef;
typedef RTID<ResourceRef>               ResourceID;
typedef FreeVector<ResourceRef>         ResourcePool;

class Texture {
	friend class Device;
	friend class CommandList;

public:
	using Pool = FreeVector<Texture>;

	enum Usage {
		GENERAL,
		SHADER_READ,
		SHADER_WRITE,
		SHADER_SAMPLE,
		RENDER_TARGET,
		DEPTH_STENCIL_TARGET
	};

	struct Desc {
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		uint32_t width = 1, height = 1, depth = 1;
		uint32_t mipLevels = 1;
		uint32_t arrayLayers = 1;
		bool shaderAccess = false;
		bool mappable = false;
		Usage usage = Usage::GENERAL;
		void* viewDesc = nullptr;
	};

	Texture() = default;
	Texture(const Desc& inDesc) : m_Description(inDesc) {}

	ResourceID GetView() const { return m_View; }
	ResourceRef& GetResource() { return m_Resource; }
	ResourceRef& operator-> () { return m_Resource; }
	const Desc& GetDesc() const { return m_Description; }
	uint32_t   GetHeapIndex() const { return m_View.ToIndex(); }
	const ResourceRef& GetResource() const { return m_Resource; }

private:
	Desc m_Description = {};
	ResourceID m_View;
	ResourceRef m_Resource = nullptr;
	AllocationRef m_Allocation = nullptr;
};

class Buffer {
	friend class Device;
	friend class CommandList;

public:
	using Pool = FreeVector<Buffer>;

	enum Usage {
		UPLOAD,
		GENERAL,
		INDEX_BUFFER,
		VERTEX_BUFFER,
		ACCELERATION_STRUCTURE
	};

	struct Desc {
		size_t size = 0;
		size_t stride = 0;
		Usage usage = Usage::GENERAL;
	};

	Buffer() = default;
	Buffer(const Desc& inDesc) : m_Description(inDesc) {}

	ResourceID GetView() const { return m_View; }
	ResourceRef& operator-> () { return m_Resource; }
	const ResourceRef& operator-> () const { return m_Resource; }
	ResourceRef& GetResource() { return m_Resource; }
	const Desc& GetDesc() const { return m_Description; }

private:
	Desc m_Description = {};
	ResourceID m_View;
	ResourceRef m_Resource = nullptr;
	AllocationRef m_Allocation = nullptr;
};

typedef Buffer::Pool::ID BufferID;
typedef Texture::Pool::ID TextureID;
}