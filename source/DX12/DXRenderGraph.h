#pragma once

#include "DXDevice.h"
#include "DXCommandList.h"

namespace Raekor {
	class Scene;
	class Viewport;
}

namespace Raekor::DX {

struct TextureResource {
	TextureID mCreatedTexture;
	TextureID mResourceTexture;

	/* The returned index can be used directly in HLSL using ResourceDescriptorHeap. */
	inline uint32_t GetBindlessIndex(Device& inDevice) {
		return inDevice.GetBindlessHeapIndex(inDevice.GetTexture(mResourceTexture).GetView());
	}
};


struct ResourceBarrier {
	union {
		BufferID mBuffer;
		TextureID mTexture;
	};
	D3D12_RESOURCE_BARRIER mBarrier = {};
};


class IRenderPass {
public:
	friend class Device;
	friend class RenderGraph;

	using CreationID = uint32_t;
	
	template<typename T>
	using SetupFn = std::function<void(IRenderPass* inRenderPass, T& inData)>;
	
	template<typename T>
	using ExecFn  = std::function<void(T& inData, CommandList& inCmdList)>;

	IRenderPass(const std::string& inName) : m_Name(inName) {}

	virtual bool IsCompute() = 0;
	virtual bool IsGraphics() = 0;
	
	virtual void Setup(Device& inDevice) = 0;
	virtual void Execute(CommandList& inCmdList) = 0;

	/* Tell the graph that inTexture was created this render pass. */
	virtual void Create(TextureID inTexture) = 0;
	
	/* Tell the graph that inTexture will be read this render pass. The graph will create resource views and add barriers for it. */
	virtual [[nodiscard]] TextureResource Read(TextureID inTexture) = 0;
	
	/* Tell the graph that inTexture will be written to this render pass. The graph will create resource views and add barriers for it. 
	If it's a graphics pass, the graph will automatically deduce render/depth targets and bind them. */
	virtual [[nodiscard]] TextureResource Write(TextureID inTexture) = 0;
	
	/* Tell the graph that inTexture will be read this render pass. The graph will create resource views and add barriers for it. */
	[[nodiscard]] TextureResource Read(TextureResource inTexture)  { return Read(inTexture.mCreatedTexture);  }
	
	/* Tell the graph that inTexture will be written to this render pass. The graph will create resource views and add barriers for it.
	If it's a graphics pass, the graph will automatically deduce render/depth targets and bind them. */
	[[nodiscard]] TextureResource Write(TextureResource inTexture) { return Write(inTexture.mCreatedTexture); }

	bool IsRead(TextureID inTexture);
	bool IsWritten(TextureID inTexture);
	bool IsCreated(TextureID inTexture);

	bool IsExternal() const			{ return m_IsExternal; }
	void SetExternal(bool inValue)	{ m_IsExternal = inValue; }

	/* Reserve memory in the frame-based ring allocator. The render graph uses this reserved size to pre-allocate the ring buffer. */
	void ReserveMemory(uint32_t inSize) { m_ConstantsSize += inSize; }

	/* AddExitBarrier is exposed to the user to add manual barriers around resources they have no control over (external code like FSR2). 
	D3D12_RESOURCE_TRANSITION_BARRIER::StateAfter could be overwritten by the graph if it finds a better match during graph compilation. */
	void AddExitBarrier(const ResourceBarrier& inBarrier)  { m_ExitBarriers.push_back(inBarrier);  }
	void AddEntryBarrier(const ResourceBarrier& inBarrier) { m_EntryBarriers.push_back(inBarrier); }

	inline const std::string& GetName() const { return m_Name; }

private:
	void FlushBarriers(Device& inDevice, CommandList& inCmdList, const Slice<ResourceBarrier>& inBarriers) const;
	void SetRenderTargets(Device& inDevice, CommandList& inCmdList) const;

protected:
	std::string					  m_Name;
	uint32_t					  m_ConstantsSize = 0;
	bool						  m_IsExternal = false;

	std::vector<TextureResource>  m_ReadTextures;
	std::vector<TextureResource>  m_WrittenTextures;
	std::vector<TextureID>		  m_CreatedTextures;

	std::vector<ResourceBarrier>  m_ExitBarriers;
	std::vector<ResourceBarrier>  m_EntryBarriers;
};



template<typename T>
class RenderPass : public IRenderPass {
public:
	friend class RenderGraph;

	RenderPass(const std::string& inName,  const IRenderPass::ExecFn<T>& inExecute) :
		IRenderPass(inName),
		m_Execute(inExecute) 
	{}

	virtual void Setup(Device& inDevice) override { m_Setup(this, m_Data); }
	virtual void Execute( CommandList& inCmdList) override { m_Execute(m_Data, inCmdList); }

	T& GetData() { return m_Data; }

protected:
	T			m_Data;
	SetupFn<T>	m_Setup;
	ExecFn<T>	m_Execute;
	uint32_t	m_RefCount;
};



template<typename T>
class GraphicsRenderPass : public RenderPass<T> {
public:
	GraphicsRenderPass(Device& inDevice, const std::string& inName, const IRenderPass::ExecFn<T>& inExecuteFn) :
		RenderPass<T>(inName, inExecuteFn),
		m_Device(inDevice)
	{}

	virtual bool IsCompute() override { return false; }
	virtual bool IsGraphics() override { return true; }

	virtual void Create(TextureID inTexture) override;
	virtual [[nodiscard]] TextureResource Read(TextureID inTexture) override;
	virtual [[nodiscard]] TextureResource Write(TextureID inTexture) override;

private:
	/* Returns either the original handle or a new one pointing at a newly created texture view. */
	[[nodiscard]] TextureID GetViewForUsage(TextureID inTexture, Texture::Usage inUsage);

	Device& m_Device;
};



template<typename T>
class ComputeRenderPass : public RenderPass<T> {
public:
	ComputeRenderPass(Device& inDevice, const std::string& inName, const IRenderPass::ExecFn<T>& inExecuteFn) :
		RenderPass<T>(inName, inExecuteFn),
		m_Device(inDevice)
	{}

	virtual bool IsCompute() override { return true; }
	virtual bool IsGraphics() override { return false; }

	virtual void Create(TextureID inTexture) override;
	virtual [[nodiscard]] TextureResource Read(TextureID inTexture) override;
	virtual [[nodiscard]] TextureResource Write(TextureID inTexture) override;

private:
	/* Returns either the original handle or a new one pointing at a newly created texture view. */
	[[nodiscard]] TextureID GetViewForUsage(TextureID inTexture, Texture::Usage inUsage);

	Device& m_Device;
};



class RenderGraph {
public:
	RenderGraph(Device& inDevice, const Viewport& inViewport, uint32_t inFrameCount);
	~RenderGraph();

	template<typename T>
	const T& AddGraphicsPass(const std::string& inName, Device& inDevice, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute);

	template<typename T>
	const T& AddComputePass(const std::string& inName, Device& inDevice, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute);

	template<typename T>
	RenderPass<T>* GetPass();

	/* Clears the graph by destroying all the render passes and their associated resources. After clearing the user is free to call Compile again. */
	void Clear(Device& inDevice);

	/* Compiles the entire graph, performs validity checks, and calculates optimal barriers. */
	bool Compile(Device& inDevice);

	/* Execute the entire graph into inCmdList. inCmdList should be open (.Begin() called) already. */
	void Execute(Device& inDevice, CommandList& inCmdList, uint64_t inFrameCounter);

	/* Sets the active backbuffer. Call this once before adding any passes and once every frame!! TODO: external resource tracking functionality. */
	void SetBackBuffer(TextureID inTexture);
	
	/* Dump the entire graph to GraphViz text, can be written directly to a file and opened using the Visual Studio Code extension. */
	std::string	ToGraphVizText(const Device& inDevice) const;

	const Viewport&	GetViewport() const		{ return m_Viewport; }
	TextureID		GetBackBuffer() const	{ return m_BackBuffer; }

	uint32_t& GetPerFrameAllocatorOffset()  { return m_PerFrameAllocatorOffset; }
	RingAllocator&  GetPerPassAllocator()	{ return m_PerPassAllocator; }
	RingAllocator&  GetPerFrameAllocator()	{ return m_PerFrameAllocator; }

private:
	TextureID m_BackBuffer;
	const Viewport& m_Viewport;
	const uint32_t m_FrameCount;

	uint32_t m_PerFrameAllocatorOffset = 0;
	RingAllocator m_PerFrameAllocator;
	RingAllocator m_PerPassAllocator;
	
	std::vector<std::unique_ptr<IRenderPass>> m_RenderPasses;
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


template<typename T>
void GraphicsRenderPass<T>::Create(TextureID inTexture) {
	IRenderPass::m_CreatedTextures.push_back(inTexture);
}


template<typename T>
TextureResource GraphicsRenderPass<T>::Read(TextureID inTexture) {
	auto result = GetViewForUsage(inTexture, Texture::SHADER_READ_ONLY);

	auto resource = TextureResource{
		.mCreatedTexture = inTexture,
		.mResourceTexture = result
	};

	IRenderPass::m_ReadTextures.push_back(resource);
	return resource;
}


template<typename T> 
TextureResource GraphicsRenderPass<T>::Write(TextureID inTexture) {
	auto usage = Texture::RENDER_TARGET;

	if (m_Device.GetTexture(inTexture).GetDesc().format == DXGI_FORMAT_D32_FLOAT)
		usage = Texture::DEPTH_STENCIL_TARGET;

	auto result = GetViewForUsage(inTexture, usage);

	auto resource = TextureResource{
		.mCreatedTexture = inTexture,
		.mResourceTexture = result
	};

	IRenderPass::m_WrittenTextures.push_back(resource);
	return resource;
}


template<typename T>
TextureID GraphicsRenderPass<T>::GetViewForUsage(TextureID inTexture, Texture::Usage inUsage) {
	auto& texture = m_Device.GetTexture(inTexture);

	if (texture.GetDesc().usage == inUsage)
		return inTexture;

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	auto new_desc = Texture::Desc{ .usage = inUsage };

	if (texture.GetDesc().format == DXGI_FORMAT_D32_FLOAT) {
		srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.Texture2D.MipLevels = -1;

		new_desc.viewDesc = &srv_desc;
	}

	return m_Device.CreateTextureView(inTexture, new_desc);
}


template<typename T>
void ComputeRenderPass<T>::Create(TextureID inTexture) {
	IRenderPass::m_CreatedTextures.push_back(inTexture);
}


template<typename T>
TextureResource ComputeRenderPass<T>::Read(TextureID inTexture) {
	auto result = GetViewForUsage(inTexture, Texture::SHADER_READ_ONLY);

	auto resource = TextureResource{
		.mCreatedTexture = inTexture,
		.mResourceTexture = result
	};

	IRenderPass::m_ReadTextures.push_back(resource);
	return resource;
}


template<typename T>
TextureResource ComputeRenderPass<T>::Write(TextureID inTexture) {
	auto result = GetViewForUsage(inTexture, Texture::SHADER_READ_WRITE);

	auto resource = TextureResource{
		.mCreatedTexture = inTexture,
		.mResourceTexture = result
	};

	IRenderPass::m_WrittenTextures.push_back(resource);
	return resource;
}


template<typename T>
TextureID ComputeRenderPass<T>::GetViewForUsage(TextureID inTexture, Texture::Usage inUsage) {
	auto& texture = m_Device.GetTexture(inTexture);

	if (texture.GetDesc().usage == inUsage)
		return inTexture;

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	auto new_desc = Texture::Desc{ .usage = inUsage };

	if (texture.GetDesc().format == DXGI_FORMAT_D32_FLOAT) {
		srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.Texture2D.MipLevels = -1;

		new_desc.viewDesc = &srv_desc;
	}

	return m_Device.CreateTextureView(inTexture, new_desc);
}


template<typename T>
const T& RenderGraph::AddGraphicsPass(const std::string& inName, Device& inDevice, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute) {
	auto& pass = m_RenderPasses.emplace_back(std::make_unique<GraphicsRenderPass<T>>(inDevice, inName, inExecute));
	inSetup(pass.get(), static_cast<RenderPass<T>*>(pass.get())->GetData());
	return static_cast<RenderPass<T>*>(pass.get())->GetData();
}


template<typename T>
const T& RenderGraph::AddComputePass(const std::string& inName, Device& inDevice, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute) {
	auto& pass = m_RenderPasses.emplace_back(std::make_unique<ComputeRenderPass<T>>(inDevice, inName, inExecute));
	inSetup(pass.get(), static_cast<RenderPass<T>*>(pass.get())->GetData());
	return static_cast<RenderPass<T>*>(pass.get())->GetData();
}


template<typename T>
RenderPass<T>* RenderGraph::GetPass() {
	for (auto& renderpass : m_RenderPasses) {
		if (auto base = static_cast<RenderPass<T>*>(renderpass.get()))
			if (base->GetData().GetRTTI() == gGetRTTI<T>())
				return base;
	}

	return nullptr;
}

}