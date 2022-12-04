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
	D3D12_RESOURCE_STATES mBeforeState;
	D3D12_RESOURCE_STATES mAfterState;
};



class IRenderPass {
public:
	friend class RenderGraph;
	using CreationID = uint32_t;
	
	template<typename T>
	using SetupFn = std::function<void(IRenderPass* inRenderPass, T& inData)>;
	
	template<typename T>
	using ExecFn  = std::function<void(const T& inData, CommandList& inCmdList)>;

	IRenderPass(const std::string& inName) : m_Name(inName) {}

	virtual bool IsCompute() const = 0;
	virtual bool IsGraphics() const = 0;
	
	virtual void Setup(Device& inDevice) = 0;
	virtual void Execute(CommandList& inCmdList) = 0;

	virtual void Create(TextureID inTexture) = 0;
	virtual [[nodiscard]] TextureID Read(TextureID inTexture) = 0;
	virtual [[nodiscard]] TextureID Write(TextureID inTexture) = 0;

	bool IsRead(TextureID inTexture);
	bool IsWritten(TextureID inTexture);
	bool IsCreated(TextureID inTexture);

	/* Only used for replacing the backbuffer RTV. */
	void Replace(TextureID inOldTexture, TextureID inNewTexture);

	inline const std::string& GetName() const { return m_Name; }

private:
	void FlushBarriers(Device& inDevice, CommandList& inCmdList) const;
	void SetRenderTargets(Device& inDevice, CommandList& inCmdList) const;

public:
	std::string					  m_Name;
	std::vector<TextureID>		  m_ReadTextures;
	std::vector<TextureID>		  m_WrittenTextures;
	std::vector<TextureID>		  m_CreatedTextures;
	std::vector<ResourceBarrier>  m_ResourceBarriers;
};



template<typename T>
class RenderPass : public IRenderPass {
public:
	friend class RenderGraph;

	RenderPass(const std::string& inName, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute) :
		IRenderPass(inName),
		m_Setup(inSetup),
		m_Execute(inExecute) 
	{}

	virtual void Setup(Device& inDevice) override { m_Setup(this, m_Data); }
	virtual void Execute(CommandList& inCmdList) override { m_Execute(m_Data, inCmdList); }

	T& GetData() { return m_Data; }

private:
	T			m_Data;
	SetupFn<T>	m_Setup;
	ExecFn<T>	m_Execute;
	uint32_t	m_RefCount;
};



template<typename T>
class GraphicsRenderPass : public RenderPass<T> {
public:
	GraphicsRenderPass(Device& inDevice, const std::string& inName, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute) :
		RenderPass<T>(inName, inSetup, inExecute),
		m_Device(inDevice)
	{}

	virtual bool IsCompute() const override { return false; }
	virtual bool IsGraphics() const override { return true; }

	/* Returns either the original handle or a new one pointing at a newly created texture view. */
	[[nodiscard]] TextureID GetViewForUsage(TextureID inTexture, Texture::Usage inUsage) {
		auto& texture = m_Device.GetTexture(inTexture);
		
		if (texture.GetDesc().usage != inUsage)
			inTexture = m_Device.CreateTextureView(inTexture, Texture::Desc{ .usage = inUsage });

		return inTexture;
	}

	virtual [[nodiscard]] TextureID Read(TextureID inTexture) override {
		auto result = GetViewForUsage(inTexture, Texture::SHADER_SAMPLE);
		IRenderPass::m_ReadTextures.push_back(result);
		return result;
	}

	virtual [[nodiscard]] TextureID Write(TextureID inTexture) override { 
		auto usage = Texture::RENDER_TARGET;

		if (m_Device.GetTexture(inTexture).GetDesc().format == DXGI_FORMAT_D32_FLOAT)
			usage = Texture::DEPTH_STENCIL_TARGET;

		auto result = GetViewForUsage(inTexture, usage);
		IRenderPass::m_WrittenTextures.push_back(result);
		return result;
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
	ComputeRenderPass(Device& inDevice, const std::string& inName, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute) :
		RenderPass<T>(inName, inSetup, inExecute),
		m_Device(inDevice)
	{}

	virtual bool IsCompute() const override { return true; }
	virtual bool IsGraphics() const override { return false; }

	/* Returns either the original handle or a new one pointing at a newly created texture view. */
	[[nodiscard]] TextureID GetViewForUsage(TextureID inTexture, Texture::Usage inUsage) {
		auto& texture = m_Device.GetTexture(inTexture);

		if (texture.GetDesc().usage != inUsage) {
			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			auto new_desc = Texture::Desc{ .usage = inUsage };
			
			if (texture.GetDesc().format == DXGI_FORMAT_D32_FLOAT) {
				srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
				srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srv_desc.Texture2D.MipLevels = -1;

				new_desc.viewDesc = &srv_desc;
			}
			
			inTexture = m_Device.CreateTextureView(inTexture, new_desc);
		}

		return inTexture;
	}

	virtual [[nodiscard]] TextureID Read(TextureID inTexture) override {
		auto result = GetViewForUsage(inTexture, Texture::SHADER_SAMPLE);
		IRenderPass::m_ReadTextures.push_back(result);
		return result;
	}

	virtual [[nodiscard]] TextureID Write(TextureID inTexture) override {
		auto result = GetViewForUsage(inTexture, Texture::SHADER_WRITE);
		IRenderPass::m_WrittenTextures.push_back(result);
		return result;
	}

	virtual void Create(TextureID inTexture) override {
		IRenderPass::m_CreatedTextures.push_back(inTexture);
	}

private:
	Device& m_Device;
};



class RenderGraph {
public:
	RenderGraph(Viewport& inViewport) : m_Viewport(inViewport) {}

	template<typename T>
	const T& AddGraphicsPass( const std::string& inName, Device& inDevice, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute) {
		auto& pass = m_RenderPasses.emplace_back(std::make_unique<GraphicsRenderPass<T>>(inDevice, inName, inSetup, inExecute));
		return static_cast<RenderPass<T>*>(pass.get())->GetData();
	}

	template<typename T>
	const T& AddComputePass(const std::string& inName, Device& inDevice, const IRenderPass::SetupFn<T>& inSetup, const IRenderPass::ExecFn<T>& inExecute) {
		auto& pass = m_RenderPasses.emplace_back(std::make_unique<ComputeRenderPass<T>>(inDevice, inName, inSetup, inExecute));
		return static_cast<RenderPass<T>*>(pass.get())->GetData();
	}


	template<typename T>
	RenderPass<T>* GetPass() {
		for (auto& renderpass : m_RenderPasses) {
			auto base = static_cast<RenderPass<T>*>(renderpass.get());
			if (base->GetData().GetRTTI() == gGetRTTI<T>())
				return base;
		}

		return nullptr;
	}

	void				Compile(Device& inDevice);
	void				Execute(Device& inDevice, CommandList& inCmdList);
	inline Viewport&	GetViewport() { return m_Viewport; }

private:
	Viewport& m_Viewport;
	std::vector<std::unique_ptr<IRenderPass>> m_RenderPasses;
};

}