#include "pch.h"
#include "assets.h"

#include "dds.h"
#include "util.h"
#include "timer.h"

namespace Raekor {

const DDS_HEADER* TextureAsset::GetHeader() {
    return reinterpret_cast<DDS_HEADER*>(m_Data.data() + sizeof(DWORD));
}


char* const TextureAsset::GetData() {
    return m_Data.data() + 128;
}


uint32_t TextureAsset::GetDataSize() const { 
    return uint32_t(m_Data.size() - 128); 
}


std::string TextureAsset::sConvert(const std::string& filepath) {
    int width, height, ch;
    std::vector<stbi_uc*> mipChain;

    stbi_uc* pixels = stbi_load(filepath.c_str(), &width, &height, &ch, 4);
    mipChain.push_back(pixels);

    if (!mipChain[0]) {
        std::cout << "stb failed " << filepath << '\n';
        return {};
    }

    if (width % 4 != 0 || height % 4 != 0) {
        std::cout << "Image " << Path(filepath).filename() << " with resolution " << width << 'x' << height << " is not a power of 2 resolution.\n";
        return {};
    }

    // TODO: gpu mip mapping, cant right now because assets are loaded in parallel but OpenGL can't do multithreading

    // mips down to 2x2 but DXT works on 4x4 so we subtract one level
    int mipmapLevels = (int)std::max(std::floor(std::log2(std::max(width, height))) - 1, 0.0);

    int actual_mip_count = mipmapLevels;

    for (size_t i = 1; i < mipmapLevels; i++) {
        glm::ivec2 prevSize = { width >> (i - 1), height >> (i - 1) };
        glm::ivec2 curSize = { width >> i, height >> i };

        if (curSize.x % 4 != 0 || curSize.y % 4 != 0) {
            std::cout << "Image " << Path(filepath).filename() << " with MIP resolution " << curSize.x << 'x' << curSize.y << " is not a power of 2 resolution.\n";
            actual_mip_count = i;
            break;
        }

        mipChain.push_back((stbi_uc*)malloc(curSize.x * curSize.y * 4));
        stbir_resize_uint8(mipChain[i - 1], prevSize.x, prevSize.y, 0, mipChain[i], curSize.x, curSize.y, 0, 4);
    }

    std::vector<unsigned char> ddsBuffer(128);
    size_t offset = 128;

    for (size_t i = 0; i < actual_mip_count; i++) {
        glm::ivec2 curSize = { width >> i, height >> i };

        if (curSize.x % 4 != 0 || curSize.y % 4 != 0) {
            std::cout << "Image " << Path(filepath).filename() << " with MIP resolution " << width << 'x' << height << " is not a power of 2 resolution.\n";
            break;
        }

        // make room for the new dxt mip
        ddsBuffer.resize(ddsBuffer.size() + curSize.x * curSize.y);

        // block compress
        Raekor::CompressDXT(ddsBuffer.data() + offset, mipChain[i], curSize.x, curSize.y, true);

        offset += curSize.x * curSize.y;
    }

    for (auto mip : mipChain)
        stbi_image_free(mip);

    // copy the magic number
    memcpy(ddsBuffer.data(), &DDS_MAGIC, sizeof(DDS_MAGIC));

    DDS_PIXELFORMAT pixelFormat;
    pixelFormat.dwSize = 32;
    pixelFormat.dwFlags = 0x4;
    pixelFormat.dwFourCC = MAKEFOURCC('D', 'X', 'T', '5');
    pixelFormat.dwRGBBitCount = 32;
    pixelFormat.dwRBitMask = 0xff000000;
    pixelFormat.dwGBitMask = 0x00ff0000;
    pixelFormat.dwBBitMask = 0x0000ff00;
    pixelFormat.dwABitMask = 0x000000ff;
    
    // fill out the header
    DDS_HEADER header;
    header.dwSize = 124;
    header.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_MIPMAPCOUNT | DDSD_LINEARSIZE;
    header.dwHeight = height;
    header.dwWidth = width;
    header.dwPitchOrLinearSize = std::max(1, ((width + 3) / 4)) * std::max(1, ((height + 3) / 4)) * 16;
    header.dwDepth = 0;
    header.dwMipMapCount = actual_mip_count;
    header.ddspf = pixelFormat;
    header.dwCaps = DDSCAPS_TEXTURE |DDSCAPS_COMPLEX | DDSCAPS_MIPMAP;

    // copy the header
    memcpy(ddsBuffer.data() + 4, &header, sizeof(DDS_HEADER));

    // write to disk
    std::string outFileName = "assets/" + std::filesystem::path(filepath).stem().string() + ".dds";
    std::ofstream outFile(outFileName, std::ios::binary | std::ios::ate);
    outFile.write((const char*)ddsBuffer.data(), ddsBuffer.size());

    return outFileName;
}


bool TextureAsset::Load(const std::string& filepath) {
    if (filepath.empty() || !FileSystem::exists(filepath))
        return false;

    std::ifstream file(filepath, std::ios::binary);

    //constexpr size_t twoMegabytes = 2097152;
    //std::vector<char> scratch(twoMegabytes);
    //file.rdbuf()->pubsetbuf(scratch.data(), scratch.size());

    m_Data.resize(FileSystem::file_size(filepath));
    file.read(m_Data.data(), m_Data.size());

    DWORD magicNumber;
    memcpy(&magicNumber, m_Data.data(), sizeof(DWORD));

    if (magicNumber != DDS_MAGIC) {
        std::cerr << "File " << filepath << " not a DDS file!\n";;
        return false;
    }

    return true;
}


Assets::Assets() {
    if (!FileSystem::exists("assets"))
        FileSystem::create_directory("assets");
}


void Assets::ReleaseUnreferenced() {
    std::vector<std::string> keys;

    for (const auto& [key, value] : *this)
        if (value.use_count() == 1)
            keys.push_back(key);

    for (const auto& key : keys)
        Release(key);
}


void Assets::Release(const std::string& filepath) {
    std::scoped_lock(m_Mutex);

    if (find(filepath) != end())
        erase(filepath);
}

ScriptAsset::~ScriptAsset() {
    if (m_HModule) 
        FreeLibrary(m_HModule);
}


std::string ScriptAsset::sConvert(const std::string& filepath) {
    // TODO: The plan is to have a seperate VS project in the solution dedicated to cpp scripts.
    //          That should compile them down to a single DLL that we can load in as asset, 
    //          possibly parsing PDB's to figure out the list of classes we can attach to entities.
    
    Timer timer;
    std::cout << "Script compile time of " << Timer::sToMilliseconds(timer.GetElapsedTime()) << " ms.\n";
    return filepath;
}


bool ScriptAsset::Load(const std::string& filepath) {
    if (filepath.empty() || !FileSystem::exists(filepath))
        return false;

    m_HModule = LoadLibraryA(filepath.c_str());

    return m_HModule != NULL;
}


void ScriptAsset::EnumerateSymbols() {
    auto current_process = GetCurrentProcess();

    MODULEINFO info;
    GetModuleInformation(current_process, m_HModule, &info, sizeof(MODULEINFO));

    std::string pdbFile = m_Path.replace_extension(".pdb").string();

    PSYM_ENUMERATESYMBOLS_CALLBACK processSymbol = [](PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext) -> BOOL {
        std::cout << "Symbol: " << pSymInfo->Name << '\n';
        return true;
    };

    PSYM_ENUMERATESYMBOLS_CALLBACK callback = [](PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext) -> BOOL {
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

    if (result) {
        SymEnumTypes(current_process, (ULONG64)result, callback, NULL);
        SymEnumSymbols(current_process, (DWORD64)result, NULL, processSymbol, NULL);
        SymUnloadModule(current_process, (DWORD64)result);
    }
}

} // raekor

