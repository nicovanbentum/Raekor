#pragma once

#include "DXDevice.h"
#include "DXCommandList.h"

namespace Raekor {
	class Scene;
	class Viewport;
}

namespace Raekor::DX {

struct ResourceBarrier {
	union {
		BufferID mBuffer;
		TextureID mTexture;
	};
	std::string mDebugName;
	D3D12_RESOURCE_BARRIER mBarrier = {};
};


enum class RenderPassType {
	GRAPHICS_PASS,
	COMPUTE_PASS
};


struct TextureResource {
	TextureID mCreatedTexture;
	TextureID mResourceTexture;

	uint32_t GetBindlessIndex(Device& inDevice) {
		return inDevice.GetBindlessHeapIndex(inDevice.GetTexture(mResourceTexture).GetView());
	}
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

	virtual RenderPassType GetRenderPassType() const = 0;
	
	virtual void Setup(Device& inDevice) = 0;
	virtual void Execute(CommandList& inCmdList) = 0;

	virtual void Create(TextureID inTexture) = 0;
	virtual [[nodiscard]] TextureResource Read(TextureID inTexture) = 0;
	virtual [[nodiscard]] TextureResource Write(TextureID inTexture) = 0;

	[[nodiscard]] TextureResource Read(TextureResource inTexture)  { return Read(inTexture.mCreatedTexture);  }
	[[nodiscard]] TextureResource Write(TextureResource inTexture) { return Write(inTexture.mCreatedTexture); }

	bool IsRead(TextureID inTexture);
	bool IsWritten(TextureID inTexture);
	bool IsCreated(TextureID inTexture);

	void AddExitBarrier(const ResourceBarrier& inBarrier)  { m_ExitBarriers.push_back(inBarrier);  }
	void AddEntryBarrier(const ResourceBarrier& inBarrier) { m_EntryBarriers.push_back(inBarrier); }

	inline const std::string& GetName() const { return m_Name; }

private:
	void FlushBarriers(Device& inDevice, CommandList& inCmdList, const Slice<ResourceBarrier>& inBarriers) const;
	void SetRenderTargets(Device& inDevice, CommandList& inCmdList) const;

protected:
	std::string					  m_Name;

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

	virtual RenderPassType GetRenderPassType() const override { return RenderPassType::GRAPHICS_PASS; }


	/* Returns either the original handle or a new one pointing at a newly created texture view. */
	[[nodiscard]] TextureID GetViewForUsage(TextureID inTexture, Texture::Usage inUsage) {
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

	virtual [[nodiscard]] TextureResource Read(TextureID inTexture) override {
		auto result = GetViewForUsage(inTexture, Texture::SHADER_READ_ONLY);
		
		auto resource = TextureResource { 
			.mCreatedTexture = inTexture, 
			.mResourceTexture = result 
		};

		IRenderPass::m_ReadTextures.push_back(resource);
		return resource;
	}

	virtual [[nodiscard]] TextureResource Write(TextureID inTexture) override {
		auto usage = Texture::RENDER_TARGET;

		if (m_Device.GetTexture(inTexture).GetDesc().format == DXGI_FORMAT_D32_FLOAT)
			usage = Texture::DEPTH_STENCIL_TARGET;

		auto result = GetViewForUsage(inTexture, usage);

		auto resource = TextureResource {
			.mCreatedTexture = inTexture,
			.mResourceTexture = result
		};

		IRenderPass::m_WrittenTextures.push_back(resource);
		return resource;
	}

	virtual void Create(TextureID inTexture) override { 
		IRenderPass::m_CreatedTextures.push_back(inTexture);
	}

private:
	Device& m_Device;
};



template<typename T>
class ComputeRenderPass : public RenderPass<T> {
public:
	ComputeRenderPass(Device& inDevice, const std::string& inName, const IRenderPass::ExecFn<T>& inExecuteFn) :
		RenderPass<T>(inName, inExecuteFn),
		m_Device(inDevice)
	{}

	virtual RenderPassType GetRenderPassType() const override { return RenderPassType::COMPUTE_PASS; }

	/* Returns either the original handle or a new one pointing at a newly created texture view. */
	[[nodiscard]] TextureID GetViewForUsage(TextureID inTexture, Texture::Usage inUsage) {
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

	virtual [[nodiscard]] TextureResource Read(TextureID inTexture) override {
		auto result = GetViewForUsage(inTexture, Texture::SHADER_READ_ONLY);

		auto resource = TextureResource{
			.mCreatedTexture = inTexture,
			.mResourceTexture = result
		};

		IRenderPass::m_ReadTextures.push_back(resource);
		return resource;
	}

	virtual [[nodiscard]] TextureResource Write(TextureID inTexture) override {
		auto result = GetViewForUsage(inTexture, Texture::SHADER_READ_WRITE);

		auto resource = TextureResource {
			.mCreatedTexture = inTexture,
			.mResourceTexture = result
		};

		IRenderPass::m_WrittenTextures.push_back(resource);
		return resource;
	}

	virtual void Create(TextureID inTexture) override {
		IRenderPass::m_CreatedTextures.push_back(inTexture);
	}

private:
	Device& m_Device;
};



class RenderGraph {
public:
	RenderGraph(Device& inDevice, const Viewport& inViewport);

	template<typename T>
	const T& AddGraphicsPass(const std::string& inName, Device& inDevice, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute) {
		auto& pass = m_RenderPasses.emplace_back(std::make_unique<GraphicsRenderPass<T>>(inDevice, inName, inExecute));
		inSetup(pass.get(), static_cast<RenderPass<T>*>(pass.get())->GetData());
		return static_cast<RenderPass<T>*>(pass.get())->GetData();
	}

	template<typename T>
	const T& AddComputePass(const std::string& inName, Device& inDevice, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute) {
		auto& pass = m_RenderPasses.emplace_back(std::make_unique<ComputeRenderPass<T>>(inDevice, inName, inExecute));
		inSetup(pass.get(), static_cast<RenderPass<T>*>(pass.get())->GetData());
		return static_cast<RenderPass<T>*>(pass.get())->GetData();
	}


	template<typename T>
	RenderPass<T>* GetPass() {
		for (auto& renderpass : m_RenderPasses) {
			if(auto base = static_cast<RenderPass<T>*>(renderpass.get()))
				if (base->GetData().GetRTTI() == gGetRTTI<T>())
					return base;
		}

		return nullptr;
	}

	void				Clear(Device& inDevice);
	bool				Compile(Device& inDevice);
	void				Execute(Device& inDevice, CommandList& inCmdList, bool isFirstFrame);

	/* Sets the active backbuffer. Call this once before adding any passes and once every frame!! */
	const Viewport&	GetViewport() { return m_Viewport; }
	void			SetBackBuffer(TextureID inTexture);
	TextureID		GetBackBuffer() const { return m_BackBuffer; }

	std::string		GetGraphViz(const Device& inDevice) const;

private:
	const Viewport& m_Viewport;

	TextureID m_BackBuffer;
	BufferID m_FrameConstantsBuffer;
	std::vector<std::unique_ptr<IRenderPass>> m_RenderPasses;
};

}