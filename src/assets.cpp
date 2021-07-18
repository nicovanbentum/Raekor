#include "pch.h"
#include "assets.h"

#include "dds.h"

namespace Raekor
{

TextureAsset::TextureAsset(const std::string& filepath)
    : Asset(filepath) {
}

//////////////////////////////////////////////////////////////////////////////////////////////////

DDS_HEADER TextureAsset::getHeader() {
    return *reinterpret_cast<DDS_HEADER*>(data.data() + sizeof(DWORD));
}

//////////////////////////////////////////////////////////////////////////////////////////////////

char* const TextureAsset::getData() {
    return data.data() + 128;
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

    int mipmapLevels = 1 + (int)std::floor(std::log2(std::max(width, height)));

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

    std::ifstream file(filepath, std::ios::binary);

    file.seekg(0, std::ios::end);
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

} // raekor

