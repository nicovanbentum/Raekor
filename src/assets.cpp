#include "pch.h"
#include "assets.h"

#include "dds.h"
#include "util.h"
#include "timer.h"
#include "vswhere.h"

namespace Raekor
{

TextureAsset::TextureAsset(const std::string& filepath)
    : Asset(filepath) {
}

//////////////////////////////////////////////////////////////////////////////////////////////////

const DDS_HEADER* TextureAsset::header() {
    return reinterpret_cast<DDS_HEADER*>(data.data() + sizeof(DWORD));
}

//////////////////////////////////////////////////////////////////////////////////////////////////

char* const TextureAsset::getData() {
    return data.data() + 128;
}



uint32_t TextureAsset::getDataSize() const { 
    return data.size() - 128; 
}

//////////////////////////////////////////////////////////////////////////////////////////////////

std::string TextureAsset::convert(const std::string& filepath) {
    int width, height, ch;
    std::vector<stbi_uc*> mipChain;
    mipChain.push_back(stbi_load(filepath.c_str(), &width, &height, &ch, 4));

    if (!mipChain[0]) {
        std::cout << "stb failed " << filepath << std::endl;
        return {};
    }

    // TODO: gpu mip mapping, cant right now because assets are loaded in parallel but OpenGL can't do multithreading

    // mips down to 2x2 but DXT works on 4x4 so we subtract one level
    int mipmapLevels = (int)std::floor(std::log2(std::max(width, height))) - 1;

    for (size_t i = 1; i < mipmapLevels; i++) {
        glm::ivec2 prevSize = { width >> (i - 1), height >> (i - 1) };
        glm::ivec2 curSize = { width >> i, height >> i };
        mipChain.push_back((stbi_uc*)malloc(curSize.x * curSize.y * 4));
        stbir_resize_uint8(mipChain[i - 1], prevSize.x, prevSize.y, 0, mipChain[i], curSize.x, curSize.y, 0, 4);
    }

    std::vector<unsigned char> ddsBuffer(128);
    size_t offset = 128;

    for (size_t i = 0; i < mipmapLevels; i++) {
        glm::ivec2 curSize = { width >> i, height >> i };

        // make room for the new dxt mip
        ddsBuffer.resize(ddsBuffer.size() + curSize.x * curSize.y);

        // block compress
        Raekor::rygCompress(ddsBuffer.data() + offset, mipChain[i], curSize.x, curSize.y, true);

        offset += curSize.x * curSize.y;
    }

    for (auto mip : mipChain) {
        stbi_image_free(mip);
    }

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
    header.dwMipMapCount = mipmapLevels;
    header.ddspf = pixelFormat;
    header.dwCaps = DDSCAPS_TEXTURE |DDSCAPS_COMPLEX | DDSCAPS_MIPMAP;

    // copy the header
    memcpy(ddsBuffer.data() + 4, &header, sizeof(DDS_HEADER));

    // write to disk
    const std::string outFileName = "assets/" + std::filesystem::path(filepath).stem().string() + ".dds";
    std::ofstream outFile(outFileName, std::ios::binary | std::ios::ate);
    outFile.write((const char*)ddsBuffer.data(), ddsBuffer.size());

    return outFileName;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

bool TextureAsset::load(const std::string& filepath) {
    if (filepath.empty() || !fs::exists(filepath)) {
        return false;
    }

    std::ifstream file(filepath, std::ios::binary | std::ios::ate);

    constexpr size_t twoMegabytes = 2097152;
    std::vector<char> scratch(twoMegabytes);
    file.rdbuf()->pubsetbuf(scratch.data(), scratch.size());

    data.resize(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(data.data(), data.size());

    DWORD magicNumber;
    memcpy(&magicNumber, data.data(), sizeof(DWORD));

    if (magicNumber != DDS_MAGIC) {
        std::cerr << "File " << filepath << " not a DDS file!\n";;
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

Assets::Assets() {
    if (!fs::exists("assets")) {
        fs::create_directory("assets");
    }
}



ScriptAsset::ScriptAsset(const std::string& filepath) : Asset(filepath) {}

ScriptAsset::~ScriptAsset() {
    if (hmodule) FreeLibrary(hmodule);
}



std::string ScriptAsset::convert(const std::string& filepath) {
    Find_Result vswhere = find_visual_studio_and_windows_sdk();
    
    std::string visualStudioPath = wchar_to_std_string(vswhere.vs_exe_path);
    std::cout << visualStudioPath << '\n';
    free_resources(&vswhere);

    std::string vcvarsall = "\"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat\" x64";
    system(std::string('"' + vcvarsall + '"').c_str());

    auto dll = fs::path(filepath);
    dll.replace_extension(".dll");

    std::string includeDirs =
        " /I\"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Tools\\MSVC\\14.28.29910\\include\""
        " /I\"C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.19041.0\\ucrt\""
        " /I\"C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.19041.0\\um\""
        " /I\"C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.19041.0\\shared\""
        " /I\"C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.19041.0\\winrt\""
        " /I\"C:\\VS-Projects\\Raekor\\src\\headers\""
        " /I\"%VULKAN_SDK%\\Include\""
        " /I\"C:\\VS-Projects\\Raekor\\dependencies\\stb\""
        " /I\"C:\\VS-Projects\\Raekor\\dependencies\\imgui\""
        " /I\"C:\\VS-Projects\\Raekor\\dependencies\\glm\\glm\""
        " /I\"C:\\VS-Projects\\Raekor\\dependencies\\ImGuizmo\""
        " /I\"C:\\VS-Projects\\Raekor\\dependencies\\gl3w\\include\""
        " /I\"C:\\VS-Projects\\Raekor\\dependencies\\cereal\\include\""
        " /I\"C:\\VS-Projects\\Raekor\\dependencies\\imgui\\backends\""
        " /I\"C:\\VS-Projects\\Raekor\\dependencies\\ChaiScript\\include\""
        " /I\"C:\\VS-Projects\\Raekor\\dependencies\\entt\\src\""
        " /I\"C:\\VS-Projects\\Raekor\\dependencies\\VulkanMemoryAllocator\\src\""
        " /I\"C:\\VS-Projects\\Raekor\\dependencies\\IconFontCppHeaders\""
        " /I\"C:\\VS-Projects\\Raekor\\vcpkg_installed\\x64-windows-static\\include\\SDL2\""
        " /I\"C:\\VS-Projects\\Raekor\\dependencies\\glad\\GL\\include\""
        " /I\"C:\\VS-Projects\\Raekor\\vcpkg_installed\\x64-windows-static\\include\\physx\""
        " /I\"C:\\VS-Projects\\Raekor\\dependencies\\SPIRV-Reflect\""
        " /I\"C:\\VS-Projects\\Raekor\\vcpkg_installed\\x64-windows-static\\include\" ";

#if RAEKOR_DEBUG
    std::string clOptions = "/D \"_DEBUG\" /MDd ";
#else
    std::string clOptions = "/D \"NDEBUG\" /MD ";
#endif
    std::string command = "\"" + visualStudioPath +  "\\cl.exe\"" + includeDirs + clOptions + "/std:c++17 /GR- /EHsc \"" + filepath +
        "\" /link "
        "/LIBPATH:\"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Tools\\MSVC\\14.28.29910\\lib\\x64\""
        " /LIBPATH:\"C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.19041.0\\um\\x64\""
        " /LIBPATH:\"C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.19041.0\\ucrt\\x64\""
        " /DLL /OUT:\"" + dll.string() + "\"";

    std::cout << command << '\n';

    Timer timer;
    timer.start();

    int result = system(std::string("\"" + command + "\"").c_str());

    std::cout << "Script compile time of " << timer.stop() << " ms.\n";

    return result == 0 ? dll.string() : std::string();
}



bool ScriptAsset::load(const std::string& filepath) {
    if (filepath.empty() || !fs::exists(filepath)) {
        return false;
    }

    hmodule = LoadLibraryA(filepath.c_str());

    return hmodule != NULL;
}

void ScriptAsset::enumerateSymbols() {
    MODULEINFO info;
    GetModuleInformation(GetCurrentProcess(), hmodule, &info, sizeof(MODULEINFO));

    std::string pdbFile = m_path.replace_extension(".pdb").string();

    PSYM_ENUMERATESYMBOLS_CALLBACK processSymbol = [](PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext) -> BOOL {
        std::cout << "Symbol: " << pSymInfo->Name << '\n';
        return true;
    };

    PSYM_ENUMERATESYMBOLS_CALLBACK callback = [](PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext) -> BOOL {
        std::cout << "User type: " << pSymInfo->Name << '\n';
        return true;
    };

    if (!SymInitialize(GetCurrentProcess(), NULL, FALSE)) {
        std::cerr << "Failed to initialize symbol handler \n";
    }


    DWORD64 result = SymLoadModuleEx(GetCurrentProcess(), NULL, pdbFile.c_str(), NULL, (DWORD64)hmodule, info.SizeOfImage, NULL, NULL);

    if (result == 0 && GetLastError() != 0) {
        std::cout << "Failed" << std::endl;
    }     else if (result == 0 && GetLastError() == 0) {
        std::cout << "Already loaded" << std::endl;
    }

    if (result) {
        SymEnumTypes(GetCurrentProcess(), (ULONG64)result, callback, NULL);
        SymEnumSymbols(GetCurrentProcess(), (DWORD64)result, NULL, processSymbol, NULL);
        SymUnloadModule(GetCurrentProcess(), (DWORD64)result);
    }
}

} // raekor

