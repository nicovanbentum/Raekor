#include "pch.h"
#include "assets.h"

#include "dds.h"
#include "util.h"
#include "timer.h"

namespace Raekor {

std::string TextureAsset::sConvert(const std::string& filepath)
{
	auto width = 0, height = 0, ch = 0;
	auto mip_chain = std::vector<stbi_uc*>();

	auto pixels = stbi_load(filepath.c_str(), &width, &height, &ch, 4);
	mip_chain.push_back(pixels);

	if (!mip_chain[0])
	{
		std::cout << "stb failed " << filepath << '\n';
		return {};
	}

	if (width % 4 != 0 || height % 4 != 0)
	{
		std::cout << "Image " << Path(filepath).filename() << " with resolution " << width << 'x' << height << " is not a power of 2 resolution.\n";
		const auto aw = gAlignUp(width, 4), ah = gAlignUp(height, 4);

		mip_chain.push_back((stbi_uc*)malloc(aw * ah * 4));
		stbir_resize_uint8_linear(mip_chain[0], width, height, 0, mip_chain[1], aw, ah, 0, (stbir_pixel_layout)4);

		stbi_image_free(mip_chain[0]);
		mip_chain[0] = mip_chain[1];
		mip_chain.pop_back();
		width = aw, height = ah;

		// return {};
	}

	// TODO: gpu mip mapping, cant right now because assets are loaded in parallel but OpenGL can't do multithreading

	// mips down to 2x2 but DXT works on 4x4 so we subtract one level
	auto nr_of_mips = (int)std::max(std::floor(std::log2(std::max(width, height))) - 1, 0.0);
	auto actual_mip_count = nr_of_mips;

	for (uint32_t i = 1; i < nr_of_mips; i++)
	{
		const auto mip_size = glm::ivec2(width >> i, height >> i);
		const auto prev_mip_size = glm::ivec2(width >> ( i - 1 ), height >> ( i - 1 ));

		if (mip_size.x % 4 != 0 || mip_size.y % 4 != 0)
		{
			std::cout << "[Assets] Image " << Path(filepath).filename() << " with MIP resolution " << mip_size.x << 'x' << mip_size.y << " is not a power of 2 resolution.\n";
			actual_mip_count = i;
			break;
		}

		mip_chain.push_back((stbi_uc*)malloc(mip_size.x * mip_size.y * 4));
		stbir_resize_uint8_linear(mip_chain[i - 1], prev_mip_size.x, prev_mip_size.y, 0, mip_chain[i], mip_size.x, mip_size.y, 0, (stbir_pixel_layout)4);
	}

	std::vector<unsigned char> dds_buffer(128);
	uint32_t offset = 128;

	for (uint32_t i = 0; i < actual_mip_count; i++)
	{
		const auto mip_dimensions = glm::ivec2(width >> i, height >> i);

		if (mip_dimensions.x % 4 != 0 || mip_dimensions.y % 4 != 0)
		{
			std::cout << "[Assets] Image " << Path(filepath).filename() << " with MIP resolution " << width << 'x' << height << " is not a power of 2 resolution.\n";
			break;
		}

		// make room for the new dxt mip
		dds_buffer.resize(dds_buffer.size() + mip_dimensions.x * mip_dimensions.y);

		// block compress
		Raekor::CompressDXT(dds_buffer.data() + offset, mip_chain[i], mip_dimensions.x, mip_dimensions.y, true);

		offset += mip_dimensions.x * mip_dimensions.y;
	}

	for (auto mip : mip_chain)
		stbi_image_free(mip);

	// copy the magic number
	memcpy(dds_buffer.data(), &DDS_MAGIC, sizeof(DDS_MAGIC));

	DDS_PIXELFORMAT pixel_format = {};
	pixel_format.dwSize = 32;
	pixel_format.dwFlags = 0x4;
	pixel_format.dwFourCC = DDS_FORMAT_DXT5;
	pixel_format.dwRBitMask = 0xff000000;
	pixel_format.dwGBitMask = 0x00ff0000;
	pixel_format.dwBBitMask = 0x0000ff00;
	pixel_format.dwABitMask = 0x000000ff;

	// fill out the header
	DDS_HEADER header = {};
	header.dwSize = 124;
	header.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_MIPMAPCOUNT | DDSD_LINEARSIZE;
	header.dwHeight = height;
	header.dwWidth = width;
	header.dwPitchOrLinearSize = std::max(1, ( ( width + 3 ) / 4 )) * std::max(1, ( ( height + 3 ) / 4 )) * 16;
	header.dwDepth = 0;
	header.dwMipMapCount = actual_mip_count;
	header.ddspf = pixel_format;
	header.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_COMPLEX | DDSCAPS_MIPMAP;

	// copy the header
	memcpy(dds_buffer.data() + 4, &header, sizeof(DDS_HEADER));

	// write to disk
	const auto dds_file_path = sAssetsToCachedPath(filepath);
	fs::create_directories(Path(dds_file_path).parent_path());

	auto dds_file = std::ofstream(dds_file_path, std::ios::binary | std::ios::ate);
	dds_file.write((const char*)dds_buffer.data(), dds_buffer.size());

	return dds_file_path;
}


bool TextureAsset::Load(const std::string& filepath)
{
	if (filepath.empty() || !fs::exists(filepath) || !fs::is_regular_file(filepath))
		return false;

	auto file = std::ifstream(filepath, std::ios::binary);

	//constexpr size_t twoMegabytes = 2097152;
	//std::vector<char> scratch(twoMegabytes);
	//file.rdbuf()->pubsetbuf(scratch.data(), scratch.size());

	m_Data.resize(fs::file_size(filepath));
	file.read(m_Data.data(), m_Data.size());

	DWORD magic_number;
	memcpy(&magic_number, m_Data.data(), sizeof(DWORD));

	if (magic_number != DDS_MAGIC)
	{
		std::cerr << "File " << filepath << " not a DDS file!\n";;
		return false;
	}

	if (GetHeader()->ddspf.dwFourCC == DDS_FORMAT_DX10)
		m_IsExtendedDX10 = true;
	
	return true;
}


Assets::Assets()
{
	if (!fs::exists("assets"))
		fs::create_directory("assets");
}


void Assets::ReleaseUnreferenced()
{
	std::vector<std::string> keys;

	for (const auto& [key, value] : *this)
		if (value.use_count() == 1)
			keys.push_back(key);

	{
		std::scoped_lock lock(m_Mutex);
		for (const auto& key : keys)
			erase(key);
	}
}


void Assets::Release(const std::string& filepath)
{
	std::scoped_lock lock(m_Mutex);

	if (find(filepath) != end())
		erase(filepath);
}

ScriptAsset::~ScriptAsset()
{
	if (m_HModule)
		FreeLibrary(m_HModule);
}


Path ScriptAsset::sConvert(const Path& inPath)
{
	// TODO: The plan is to have a seperate VS project in the solution dedicated to cpp scripts.
	//          That should compile them down to a single DLL that we can load in as asset, 
	//          possibly parsing PDB's to figure out the list of classes we can attach to entities.
	auto abs_path = inPath;
	const auto dest = "assets" / abs_path.filename();
	const auto pdb_path = abs_path.replace_extension(".pdb");

	fs::copy(inPath, "assets" / abs_path.filename());
	fs::copy(pdb_path, "assets" / pdb_path.filename());
	return dest;
}


bool ScriptAsset::Load(const std::string& inPath)
{
	if (inPath.empty() || !fs::exists(inPath))
		return false;

	m_HModule = LoadLibraryA(inPath.c_str());

	return m_HModule != NULL;
}


void ScriptAsset::EnumerateSymbols()
{
	auto current_process = GetCurrentProcess();

	MODULEINFO info;
	GetModuleInformation(current_process, m_HModule, &info, sizeof(MODULEINFO));

	std::string pdbFile = m_Path.replace_extension(".pdb").string();

	PSYM_ENUMERATESYMBOLS_CALLBACK processSymbol = [](PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext) -> BOOL
	{
		std::cout << "Symbol: " << pSymInfo->Name << '\n';
		return true;
	};

	PSYM_ENUMERATESYMBOLS_CALLBACK callback = [](PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext) -> BOOL
	{
		std::cout << "User type: " << pSymInfo->Name << '\n';
		return true;
	};

	if (!SymInitialize(current_process, NULL, FALSE))
		std::cerr << "Failed to initialize symbol handler \n";

	DWORD64 result = SymLoadModuleEx(current_process, NULL, pdbFile.c_str(), NULL, (DWORD64)m_HModule, info.SizeOfImage, NULL, NULL);

	if (result == 0 && GetLastError() != 0)
		std::cout << "Failed \n";
	else if (result == 0 && GetLastError() == 0)
		std::cout << "Already loaded \n";

	if (result)
	{
		SymEnumTypes(current_process, (ULONG64)result, callback, NULL);
		SymEnumSymbols(current_process, (DWORD64)result, NULL, processSymbol, NULL);
		SymUnloadModule(current_process, (DWORD64)result);
	}
}

} // raekor

