#include "pch.h"
#include "DXShader.h"
#include "DXUtil.h"
#include "Raekor/async.h"

namespace Raekor::DX12 {

ShaderCompiler g_ShaderCompiler;
SystemShadersDX12 g_SystemShaders;

RTTI_DEFINE_ENUM(EShaderProgramType)
{
	RTTI_DEFINE_ENUM_MEMBER(EShaderProgramType, SERIALIZE_ALL, "Invalid", SHADER_PROGRAM_INVALID);
	RTTI_DEFINE_ENUM_MEMBER(EShaderProgramType, SERIALIZE_ALL, "Graphics", SHADER_PROGRAM_GRAPHICS);
	RTTI_DEFINE_ENUM_MEMBER(EShaderProgramType, SERIALIZE_ALL, "Compute", SHADER_PROGRAM_COMPUTE);
}

RTTI_DEFINE_TYPE(IResource)
{
	// Base class interface only
}

RTTI_DEFINE_TYPE(ShaderProgram)
{
	RTTI_DEFINE_TYPE_INHERITANCE(ShaderProgram, IResource);

	RTTI_DEFINE_MEMBER(ShaderProgram, SERIALIZE_JSON, "Defines", mDefines);
	RTTI_DEFINE_MEMBER(ShaderProgram, SERIALIZE_JSON, "Vertex Shader File", mVertexShaderFilePath);
	RTTI_DEFINE_MEMBER(ShaderProgram, SERIALIZE_JSON, "Pixel Shader File", mPixelShaderFilePath);
	RTTI_DEFINE_MEMBER(ShaderProgram, SERIALIZE_JSON, "Compute Shader File", mComputeShaderFilePath);

	RTTI_DEFINE_MEMBER(ShaderProgram, SERIALIZE_BINARY, "Program Type", mProgramType);
	RTTI_DEFINE_MEMBER(ShaderProgram, SERIALIZE_BINARY, "Vertex Shader", mVertexShader);
	RTTI_DEFINE_MEMBER(ShaderProgram, SERIALIZE_BINARY, "Pixel Shader", mPixelShader);
	RTTI_DEFINE_MEMBER(ShaderProgram, SERIALIZE_BINARY, "Compute Shader", mComputeShader);
}


RTTI_DEFINE_TYPE(SystemShadersDX12)
{
	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "ImGui Shader", mImGuiShader);
	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Grass Shader", mGrassShader);
	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Shader", mGBufferShader);
	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Lighting Shader", mLightingShader);
	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Downsample Shader", mDownsampleShader);
	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Debug Lines Shader", mDebugLinesShader);
	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Final Compose Shader", mFinalComposeShader);

	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Probe Debug Shader", mProbeDebugShader);
	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Probe Trace Shader", mProbeTraceShader);
	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Probe Update Depth Shader", mProbeUpdateDepthShader);
	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "Probe Update Irradiance Shader", mProbeUpdateIrradianceShader);

	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "RT Shadows Shader", mRTShadowsShader);
	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "RT Reflections Shader", mRTReflectionsShader);
	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "RT Indirect Diffuse Shader", mRTIndirectDiffuseShader);
	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "RT Ambient Occlusion Shader", mRTAmbientOcclusionShader);


	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Debug Depth Shader", mGBufferDebugDepthShader);
	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Debug Albedo Shader", mGBufferDebugAlbedoShader);
	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Debug Normals Shader", mGBufferDebugNormalsShader);
	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Debug Metallic Shader", mGBufferDebugMetallicShader);
	RTTI_DEFINE_MEMBER(SystemShadersDX12, SERIALIZE_ALL, "GBuffer Debug Roughness Shader", mGBufferDebugRoughnessShader);
}


bool ShaderProgram::GetGraphicsProgram(CD3DX12_SHADER_BYTECODE& ioVertexShaderByteCode, CD3DX12_SHADER_BYTECODE& ioPixelShaderByteCode) const
{
	ioVertexShaderByteCode = CD3DX12_SHADER_BYTECODE(mVertexShader.data(), mVertexShader.size());
	ioPixelShaderByteCode = CD3DX12_SHADER_BYTECODE(mPixelShader.data(), mPixelShader.size());
	return IsCompiled();
}


bool ShaderProgram::GetComputeProgram(CD3DX12_SHADER_BYTECODE& ioComputeShaderByteCode) const
{
	ioComputeShaderByteCode = CD3DX12_SHADER_BYTECODE(mComputeShader.data(), mComputeShader.size());
	return IsCompiled();
}


bool ShaderProgram::OnCompile()
{
	if (!mVertexShaderFilePath.empty() && !mPixelShaderFilePath.empty() && mComputeShaderFilePath.empty())
	{
		mProgramType = SHADER_PROGRAM_GRAPHICS;
	}
	else if (mVertexShaderFilePath.empty() && mPixelShaderFilePath.empty() && !mComputeShaderFilePath.empty())
	{
		mProgramType = SHADER_PROGRAM_COMPUTE;
	}

	if (mProgramType == SHADER_PROGRAM_INVALID)
		return false;

	auto CopyByteCode = [](const ComPtr<IDxcBlob>& inByteCode, std::vector<uint8_t>& ioResult)
	{
		if (inByteCode)
		{
			ioResult.resize(inByteCode->GetBufferSize());
			std::memcpy(ioResult.data(), inByteCode->GetBufferPointer(), inByteCode->GetBufferSize());
		}
	};

	switch (mProgramType)
	{
		case SHADER_PROGRAM_GRAPHICS:
		{
			mVertexShaderFileTime = fs::last_write_time(mVertexShaderFilePath);
			mPixelShaderFileTime = fs::last_write_time(mPixelShaderFilePath);

			auto vertex_shader_blob = g_ShaderCompiler.CompileShader(mVertexShaderFilePath, SHADER_TYPE_VERTEX, mDefines);
			auto pixel_shader_blob = g_ShaderCompiler.CompileShader(mPixelShaderFilePath, SHADER_TYPE_PIXEL, mDefines);

			if (!vertex_shader_blob || !pixel_shader_blob)
				return false;

			CopyByteCode(vertex_shader_blob, mVertexShader);
			CopyByteCode(pixel_shader_blob, mPixelShader);

			return !mVertexShader.empty() && !mPixelShader.empty();
		} break;

		case SHADER_PROGRAM_COMPUTE:
		{
			mComputeShaderFileTime = fs::last_write_time(mComputeShaderFilePath);

			auto compute_shader_blob = g_ShaderCompiler.CompileShader(mComputeShaderFilePath, SHADER_TYPE_COMPUTE, mDefines);

			if (!compute_shader_blob)
				return false;

			CopyByteCode(compute_shader_blob, mComputeShader);

			return !mComputeShader.empty();
		} break;

		default:
			return false;
	}

	return true;
}


bool ShaderProgram::IsCompiled() const
{
	switch (mProgramType)
	{
		case SHADER_PROGRAM_GRAPHICS:
			return !mVertexShader.empty() && !mPixelShader.empty();
		case SHADER_PROGRAM_COMPUTE:
			return !mComputeShader.empty();
		default:
			return false;
	}
}


ComPtr<IDxcBlob> ShaderCompiler::CompileShader(const Path& inPath, EShaderType inShaderType, const std::string& inDefines)
{

	auto utils = ComPtr<IDxcUtils> {};
	auto library = ComPtr<IDxcLibrary> {};
	auto compiler = ComPtr<IDxcCompiler3> {};
	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(utils.GetAddressOf()));
	DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(library.GetAddressOf()));
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(compiler.GetAddressOf()));

	auto include_handler = ComPtr<IDxcIncludeHandler> {};
	utils->CreateDefaultIncludeHandler(include_handler.GetAddressOf());

	auto ifs = std::ifstream(inPath);
	auto buffer = std::stringstream();
	buffer << inDefines << '\n';
	buffer << ifs.rdbuf();
	const auto source_str = buffer.str();

	auto blob = ComPtr<IDxcBlobEncoding>();
	gThrowIfFailed(library->CreateBlobWithEncodingFromPinned(source_str.c_str(), source_str.size(), CP_UTF8, blob.GetAddressOf()));

	auto arguments = std::vector<LPCWSTR> {};
	arguments.push_back(L"-E");
	arguments.push_back(L"main");

	arguments.push_back(L"-T");

	switch (inShaderType)
	{
		case SHADER_TYPE_VERTEX:  arguments.push_back(L"vs_6_6"); break;
		case SHADER_TYPE_PIXEL:   arguments.push_back(L"ps_6_6"); break;
		case SHADER_TYPE_COMPUTE: arguments.push_back(L"cs_6_6"); break;
		default: assert(false);
	}

	arguments.push_back(L"-Zi");
#ifndef NDEBUG
	arguments.push_back(L"-Qembed_debug");
	arguments.push_back(L"-Od");
#endif

	arguments.push_back(L"-I");
	arguments.push_back(L"assets/system/shaders/DirectX");

	arguments.push_back(L"-HV");
	arguments.push_back(L"2021");

	auto str_filepath = inPath.string();
	auto wstr_filepath = std::wstring(str_filepath.begin(), str_filepath.end());
	arguments.push_back(DXC_ARG_DEBUG_NAME_FOR_SOURCE);
	arguments.push_back(wstr_filepath.c_str());

	auto source_buffer = DxcBuffer 
	{
		.Ptr = blob->GetBufferPointer(),
		.Size = blob->GetBufferSize(),
		.Encoding = 0
	};

	arguments.push_back(L"-P");
	auto pp_result = ComPtr<IDxcResult> {};
	gThrowIfFailed(compiler->Compile(&source_buffer, arguments.data(), uint32_t(arguments.size()), include_handler.Get(), IID_PPV_ARGS(pp_result.GetAddressOf())));

	auto hr_status = HRESULT {};
	gThrowIfFailed(pp_result->GetStatus(&hr_status));

	auto preprocessed_hlsl = ComPtr<IDxcBlobUtf8>();
	gThrowIfFailed(pp_result->GetOutput(DXC_OUT_HLSL, IID_PPV_ARGS(preprocessed_hlsl.GetAddressOf()), nullptr));

	auto hash_key = 0ui64;

	if (m_EnableCache)
	{
		hash_key = gHashFNV1a((const char*)preprocessed_hlsl->GetBufferPointer(), preprocessed_hlsl->GetBufferSize());
		{
			auto lock = std::scoped_lock(m_ShaderCompilationMutex);
			if (m_ShaderCache.contains(hash_key))
				return m_ShaderCache.at(hash_key);
		}
	}

	arguments.pop_back(); // pop -P preprocessor flag

	source_buffer = DxcBuffer 
	{
		.Ptr = preprocessed_hlsl->GetBufferPointer(),
		.Size = preprocessed_hlsl->GetBufferSize(),
		.Encoding = 0
	};

	auto result = ComPtr<IDxcResult> {};
	gThrowIfFailed(compiler->Compile(&source_buffer, arguments.data(), uint32_t(arguments.size()), include_handler.Get(), IID_PPV_ARGS(result.GetAddressOf())));

	auto errors = ComPtr<IDxcBlobUtf8> {};
	gThrowIfFailed(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.GetAddressOf()), nullptr));

	if (errors && errors->GetStringLength() > 0)
	{
		auto lock = std::scoped_lock(m_ShaderCompilationMutex);
		auto error_c_str = static_cast<char*>( errors->GetBufferPointer() );
		std::cout << error_c_str << '\n';

		auto error_str = std::string();
		auto line_nr = 0, char_nr = 0;

		auto token = strtok(error_c_str, ":");

		if (strncmp(token, "error", strlen("error")) != 0)
		{
			token = strtok(NULL, ":"); // File path/name is ignored
			line_nr = atoi(token);
			token = strtok(NULL, ":");
			char_nr = atoi(token);
			token = strtok(NULL, ":");
			error_str = std::string(token);
		}

		token = strtok(NULL, ":");
		const auto error_msg = std::string(token);

		OutputDebugStringA(std::format("{}({}): Error: {}", fs::absolute(inPath).string(), line_nr, error_msg).c_str());

	}

	if (!SUCCEEDED(hr_status))
	{
		std::cout << std::format("Compilation {} for shader: {} \n", COUT_RED("failed"), inPath.string());
		return nullptr;
	}

	auto shader = ComPtr<IDxcBlob> {}, pdb = ComPtr<IDxcBlob> {};
	auto debug_data = ComPtr<IDxcBlobUtf16> {};
	hr_status = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(shader.GetAddressOf()), debug_data.GetAddressOf());
	hr_status = result->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(pdb.GetAddressOf()), debug_data.GetAddressOf());

	if (!SUCCEEDED(hr_status))
	{
		std::cout << std::format("Compilation {} for shader: {} \n", COUT_RED("failed"), inPath.string());
		return nullptr;
	}

	auto pdb_file = std::ofstream(inPath.parent_path() / inPath.filename().replace_extension(".pdb"));
	pdb_file.write((char*)pdb->GetBufferPointer(), pdb->GetBufferSize());

	if (m_EnableCache)
	{
		auto lock = std::scoped_lock(m_ShaderCompilationMutex);
		m_ShaderCache.insert({ hash_key , shader });
	}

	std::cout << std::format("Compilation {} for shader: {} \n", COUT_GREEN("finished"), inPath.string());

	return shader;
}


bool SystemShadersDX12::HotLoad()
{
	auto should_hotload = false;
	// g_ShaderCompiler.DisableShaderCache();

	for (const auto& member : GetRTTI())
	{
		auto& program = member->GetRef<ShaderProgram>(this);

		auto GetFileTimeStamp = [](const Path& inPath)
		{
			auto error_code = std::error_code();
			auto timestamp = fs::last_write_time(inPath, error_code);

			while (error_code)
				timestamp = fs::last_write_time(inPath, error_code);

			return timestamp;
		};

		auto should_recompile = false;

		if (!program.mVertexShaderFilePath.empty() && program.mVertexShaderFileTime < GetFileTimeStamp(program.mVertexShaderFilePath))
			should_recompile = true;

		if (!program.mPixelShaderFilePath.empty() && program.mPixelShaderFileTime < GetFileTimeStamp(program.mPixelShaderFilePath))
			should_recompile = true;

		if (!program.mComputeShaderFilePath.empty() && program.mComputeShaderFileTime < GetFileTimeStamp(program.mComputeShaderFilePath))
			should_recompile = true;

		if (should_recompile)
		{
			should_hotload = true;
			if (!program.OnCompile())
				return false;

		}
	}

	// g_ShaderCompiler.EnableShaderCache();
	return should_hotload;
}


bool SystemShadersDX12::OnCompile()
{
	for (const auto& member : GetRTTI())
	{
		g_ThreadPool.QueueJob([this, &member]()
		{
			auto& shader_program = member->GetRef<ShaderProgram>(this);
			shader_program.OnCompile();
		});
	}

	return true;
}


bool SystemShadersDX12::IsCompiled() const
{
	auto is_compiled = true;
	for (const auto& member : GetRTTI())
		is_compiled &= member->GetRef<ShaderProgram>(this).IsCompiled();

	return is_compiled;
}


} // Raekor::DX12