#pragma once

#include "DXUtil.h"

namespace Raekor::DX {

using ResourceRef	 = ComPtr<ID3D12Resource>;
using AllocationRef	 = ComPtr<D3D12MA::Allocation>;

using DescriptorPool = FreeVector<ResourceRef>;
using DescriptorID	 = DescriptorPool::ID;

class Texture {
	friend class Device;
	friend class CommandList;

public:
	using Pool = FreeVector<Texture>;

	enum Usage {
		GENERAL,
		SHADER_READ_ONLY,
		SHADER_READ_WRITE,
		RENDER_TARGET,
		DEPTH_STENCIL_TARGET
	};

	struct Desc {
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		uint32_t width = 1;
		uint32_t height = 1;
		uint32_t depth = 1;
		uint32_t mipLevels = 1;
		uint32_t arrayLayers = 1;
		bool shaderAccess = false;
		bool mappable = false;
		Usage usage = Usage::GENERAL;
		void* viewDesc = nullptr;
	};

	Texture() = default;
	Texture(const Desc& inDesc) : m_Desc(inDesc) {}

	const Desc& GetDesc() const		{ return m_Desc; }
	DescriptorID GetView() const	{ return m_View; }
	uint32_t GetHeapIndex() const	{ return m_View.ToIndex(); }

	ResourceRef& operator-> ()				{ return m_Resource; }
	const ResourceRef& operator-> () const	{ return m_Resource; }
	ResourceRef&		GetResource()		{ return m_Resource; }
	const ResourceRef&	GetResource() const	{ return m_Resource; }

private:
	Desc m_Desc = {};
	DescriptorID m_View;
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
		SHADER_READ_ONLY,
		SHADER_READ_WRITE,
		ACCELERATION_STRUCTURE,
	};

	struct Desc {
		size_t size = 0;
		size_t stride = 0;
		Usage usage = Usage::GENERAL;
		void* viewDesc = nullptr;
	};

	Buffer() = default;
	Buffer(const Desc& inDesc) : m_Desc(inDesc) {}
	~Buffer() { if (m_MappedPtr) m_Resource->Unmap(0, nullptr); }

	const Desc& GetDesc() const { return m_Desc; }
	DescriptorID GetView() const { return m_View; }
	ResourceRef& GetResource() { return m_Resource; }

	ResourceRef& operator-> () { return m_Resource; }
	const ResourceRef& operator-> () const { return m_Resource; }

private:
	Desc m_Desc = {};
	void* m_MappedPtr = nullptr;
	DescriptorID m_View;
	ResourceRef m_Resource = nullptr;
	AllocationRef m_Allocation = nullptr;
};


using BufferID = Buffer::Pool::ID;
using TextureID = Texture::Pool::ID;


D3D12_RESOURCE_STATES gGetResourceStates(Buffer::Usage inUsage);
D3D12_RESOURCE_STATES gGetResourceStates(Texture::Usage inUsage);


D3D12_DESCRIPTOR_HEAP_TYPE gGetHeapType(Buffer::Usage inUsage);
D3D12_DESCRIPTOR_HEAP_TYPE gGetHeapType(Texture::Usage inUsage);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace::Raekor