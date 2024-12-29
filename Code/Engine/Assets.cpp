#include "pch.h"
#include "Assets.h"
#include "DDS.h"
#include "Script.h"
#include "Timer.h"
#include "Maths.h"
#include "DDS.h"

namespace RK {

RTTI_DEFINE_TYPE_NO_FACTORY(Asset) {}

RTTI_DEFINE_TYPE(ScriptAsset) { RTTI_DEFINE_TYPE_INHERITANCE(ScriptAsset, Asset); }

RTTI_DEFINE_TYPE(TextureAsset) { RTTI_DEFINE_TYPE_INHERITANCE(TextureAsset, Asset); }


String TextureAsset::Convert(const String& inPath)
{
	int width = 0, height = 0, ch = 0;
	stbi_uc* mip0_pixels = stbi_load(inPath.c_str(), &width, &height, &ch, 4);

	if (mip0_pixels == nullptr)
	{
		std::cout << std::format("[STBI] Failed to load {}\n", inPath);
		return String();
	}
	
	Array<stbi_uc*> mip_chain;
	mip_chain.push_back(mip0_pixels);

	// if it's not a power of 2, instead of ruling it unusable, try to resize it to a power of 2
	if (!gIsPow2(width) || !gIsPow2(height))
	{
		const size_t aligned_width = gAlignUp(width, 32); 
		const size_t aligned_height = gAlignUp(height, 32);

		mip_chain.push_back((stbi_uc*)malloc(aligned_width * aligned_height * 4));
		stbir_resize_uint8_linear(mip_chain[0], width, height, 0, mip_chain[1], aligned_width, aligned_height, 0, STBIR_RGBA);

		stbi_image_free(mip_chain[0]);
		mip_chain[0] = mip_chain[1];
		mip_chain.pop_back();

		width = aligned_width;
		height = aligned_height;

		String filename = Path(inPath).filename().string();
		std::cout << std::format("[Assets] Image {} at {}x{} is not a power of 2.\n", filename, width, height);
	}

	// TODO: gpu mip mapping, cant right now because assets are loaded in parallel but OpenGL can't do multithreading

	// mips down to 2x2 but DXT works on 4x4 so we subtract one level
	int nr_of_mips = (int)std::max(std::floor(std::log2(std::max(width, height))) - 1, 0.0);
	int actual_mip_count = nr_of_mips;

	for (int mip_index = 1; mip_index < nr_of_mips; mip_index++)
	{
		const IVec2 mip_size = IVec2(width >> mip_index, height >> mip_index);
		const IVec2 prev_mip_size = IVec2(width >> ( mip_index - 1 ), height >> ( mip_index - 1 ));

		if (!gIsPow2(mip_size.x) || !gIsPow2(mip_size.y))
		{
			actual_mip_count = mip_index;
			String filename = Path(inPath).filename().string();
			std::cout << std::format("[Assets] Mip {} of Image {} at {}x{} is not a power of 2\n", mip_index, filename, mip_size.x, mip_size.y);
			break;
		}

		mip_chain.push_back((stbi_uc*)malloc(mip_size.x * mip_size.y * 4));
		stbir_resize_uint8_linear(mip_chain[mip_index - 1], prev_mip_size.x, prev_mip_size.y, 0, mip_chain[mip_index], mip_size.x, mip_size.y, 0, STBIR_RGBA);
	}

	uint32_t offset = 128;
	Array<unsigned char> dds_buffer(offset);

	for (int mip_index = 0; mip_index < actual_mip_count; mip_index++)
	{
		const IVec2 mip_size = IVec2(width >> mip_index, height >> mip_index);

		if (!gIsPow2(mip_size.x) || !gIsPow2(mip_size.y))
		{
			String filename = Path(inPath).filename().string();
			std::cout << std::format("[Assets] Mip {} of Image {} at {}x{} is not a power of 2\n", mip_index, filename, mip_size.x, mip_size.y);
			break;
		}

		// make room for the new dxt mip
		dds_buffer.resize(dds_buffer.size() + mip_size.x * mip_size.y);

		// compress data block
		bool is_dxt5 = true;
		unsigned char block[64];
		auto ExtractBlock = [](const unsigned char* src, int x, int y, int w, int h, unsigned char* block)
		{
			if (( w - x >= 4 ) && ( h - y >= 4 ))
			{
				src += x * 4;
				src += y * w * 4;

				for (int i = 0; i < 4; ++i)
				{
					*(unsigned int*)block = *(unsigned int*)src; block += 4; src += 4;
					*(unsigned int*)block = *(unsigned int*)src; block += 4; src += 4;
					*(unsigned int*)block = *(unsigned int*)src; block += 4; src += 4;
					*(unsigned int*)block = *(unsigned int*)src; block += 4;
					src += ( w * 4 ) - 12;
				}

				return;
			}
		};

		unsigned char* src = mip_chain[mip_index];
		unsigned char* dst = dds_buffer.data() + offset;

		for (int y = 0; y < mip_size.y; y += 4)
		{
			for (int x = 0; x < mip_size.x; x += 4)
			{
				ExtractBlock(src, x, y, mip_size.x, mip_size.y, block);
				stb_compress_dxt_block(dst, block, is_dxt5, 10);
				dst += is_dxt5 ? 16 : 8;
			}
		}

		offset += mip_size.x * mip_size.y;
	}

	for (stbi_uc* mip : mip_chain)
		stbi_image_free(mip);

	dds::Header header = {};
	dds::write_header(&header, dds::DXGI_FORMAT_BC3_UNORM, width, height, actual_mip_count);
	header.header.ddspf.dwFourCC = dds::fourcc('D', 'X', 'T', '5');

	std::memcpy(dds_buffer.data(), &header.magic, sizeof(header.magic));
	std::memcpy(dds_buffer.data() + sizeof(header.magic), &header.header, sizeof(header.header));

	// write to disk
	const String dds_file_path = GetCachedPath(inPath);
	fs::create_directories(Path(dds_file_path).parent_path());

	std::ofstream dds_file(dds_file_path, std::ios::binary | std::ios::ate);
	dds_file.write((const char*)dds_buffer.data(), dds_buffer.size());

	return dds_file_path;
}


bool TextureAsset::Save()
{
	File file(m_Path, std::ios::binary | std::ios::out);

	if (!file.is_open())
		return false;

	file.write((const char*)m_Data.data(), m_Data.size());

	return true;
}


bool TextureAsset::Load()
{
	std::ifstream file(m_Path, std::ios::binary);

	//constexpr size_t twoMegabytes = 2097152;
	//std::vector<char> scratch(twoMegabytes);
	//file.rdbuf()->pubsetbuf(scratch.data(), scratch.size());

	m_Data.resize(fs::file_size(m_Path));
	file.read((char*)m_Data.data(), m_Data.size());

	dds::Header header = dds::read_header(m_Data.data(), m_Data.size());

	if (!header.is_valid())
	{
		std::cerr << "File " << m_Path << " not a DDS file!\n";;
		return false;
	}

	return true;
}


Assets::Assets()
{
	if (!fs::exists("assets"))
		fs::create_directory("assets");
}


void Assets::ReleaseUnreferenced()
{
	Array<String> keys;

	for (const auto& [key, value] : m_Assets)
		if (value.use_count() == 1)
			keys.push_back(key);

	{
		std::scoped_lock lock(m_Mutex);
		for (const String& key : keys)
			m_Assets.erase(key);
	}
}


bool Assets::ReleaseAsset(const String& inFilePath)
{
	std::scoped_lock lock(m_Mutex);

	if (!m_Assets.contains(inFilePath))
		return false;

	m_Assets.erase(inFilePath);
	return true;
}


ScriptAsset::~ScriptAsset()
{
	if (m_HModule)
		if (!FreeLibrary((HMODULE)m_HModule))
			std::cout << std::format("FreeLibrary(\"{}\") call failed! \n", m_Path.stem().string());

	std::cout << std::format("[Assets] Unloaded {}\n", m_TempPath.string());

	std::error_code error_code;
	fs::remove(m_TempPath, error_code);
}


String ScriptAsset::Convert(const String& inPath)
{
	// TODO: The plan is to have a seperate VS project in the solution dedicated to cpp scripts.
	//          That should compile them down to a single DLL that we can load in as asset, 
	//          possibly parsing PDB's to figure out the list of classes we can attach to entities.
	Path abs_path = inPath;
	const Path dest = "assets" / abs_path.filename();
	const Path pdb_path = abs_path.replace_extension(".pdb");

	fs::copy(inPath, "assets" / abs_path.filename());
	fs::copy(pdb_path, "assets" / pdb_path.filename());
	return dest.string();
}


bool ScriptAsset::Load()
{
	std::error_code remove_error_code;
	fs::remove(m_TempPath, remove_error_code);

	if (remove_error_code)
		std::cout << std::format("[Assets] Failed to remove {}\n", m_TempPath.string());

	std::error_code copy_error_code;
	fs::copy(m_Path, m_TempPath, copy_error_code);

	if (copy_error_code)
		std::cout << std::format("[Assets] Failed to copy {} to {}\n", m_Path.string(), m_TempPath.string());

	String temp_path_str = m_TempPath.string();
	m_HModule = LoadLibraryA(temp_path_str.c_str());

	if (!m_HModule)
		return false;

	std::cout << std::format("[Assets] Loaded {}\n", temp_path_str);

	if (FARPROC address = GetProcAddress((HMODULE)m_HModule, SCRIPT_EXPORTED_FUNCTION_STR))
	{
		INativeScript::RegisterFn GetTypes = (INativeScript::RegisterFn)(address);

		Array<RTTI*> types;
		types.resize(GetTypes(nullptr));
		GetTypes(types.data());

		for (RTTI* rtti : types)
		{
			g_RTTIFactory.Register(*rtti);
			m_RegisteredTypes.push_back(rtti->GetTypeName());
		}
	}
	else
	{
		FreeLibrary((HMODULE)m_HModule);
		return false;
	}

	return true;
}


void ScriptAsset::EnumerateSymbols()
{
#if 0
	HANDLE current_process = GetCurrentProcess();

	MODULEINFO info;
	GetModuleInformation(current_process, (HMODULE)m_HModule, &info, sizeof(MODULEINFO));

	std::string pdbFile = m_Path.replace_extension(".pdb").string();

	PSYM_ENUMERATESYMBOLS_CALLBACK processSymbol = [](PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext) -> BOOL
	{
        ScriptAsset* asset = (ScriptAsset*)UserContext;

        if (strncmp(pSymInfo->Name, "Create", 6) == 0)
        {
            //std::cout << "Symbol: " << pSymInfo->Name << '\n';
        }

        return true;
	};

	PSYM_ENUMERATESYMBOLS_CALLBACK callback = [](PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext) -> BOOL
	{
        if (strncmp(pSymInfo->Name, "Create", 6) == 0)
        {
		    // std::cout << "User type: " << pSymInfo->Name << '\n';
        }
		
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
		// SymEnumTypes(current_process, (ULONG64)result, callback, NULL);
		SymEnumSymbols(current_process, (DWORD64)result, NULL, processSymbol, this);
		SymUnloadModule(current_process, (DWORD64)result);
	}
#endif
}

} // raekor

