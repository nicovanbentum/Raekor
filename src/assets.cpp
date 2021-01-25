#include "pch.h"
#include "assets.h"

#include "dds.h"
#include "components.h"

namespace Raekor {

TextureAsset::~TextureAsset() {
    delete[] data;
}

bool TextureAsset::create(const std::string& filepath) {
    int width, height, ch;
    auto stbData = stbi_load(filepath.c_str(), &width, &height, &ch, 4);

    if (!stbData) return false;

    const unsigned int ddsFileSize = 128 + width * height;

    unsigned char* ddsBuffer = new unsigned char[ddsFileSize];
    Raekor::compressDXT(ddsBuffer + 128, stbData, width, height, true);

    // ask lz4 for an upper bound on the compressed size

    // copy the magic number
    memcpy(ddsBuffer, &DDS_MAGIC, sizeof(DDS_MAGIC));

    // fill out the header
    DDS_HEADER header;
    header.dwWidth = width;
    header.dwHeight = height;

    // copy the header
    memcpy(ddsBuffer + 4, &header, sizeof(DDS_HEADER));

    int maxDstSize = LZ4_compressBound(ddsFileSize);

    // insert a single unsigned int at the start that stores the uncompressed byte size
    char* compressed = new char[sizeof(unsigned int) + maxDstSize];
    memcpy(compressed, &ddsFileSize, sizeof(unsigned int));

    std::cout << ddsFileSize << std::endl;

    // compress the texture data 
    int compressedSize = LZ4_compress_default((const char*)ddsBuffer, compressed + sizeof(unsigned int), ddsFileSize, maxDstSize);

    // write to disk
    std::ofstream outFile(std::filesystem::path(filepath).stem().string() + ".tex", std::ios::binary | std::ios::ate);
    outFile.write(compressed, compressedSize);

    delete[] ddsBuffer;
    delete[] compressed;

    return true;
}

bool TextureAsset::load(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);

    if (file.fail()) return false;

    //Read file into buffer
    std::string buffer;
    file.seekg(0, std::ios::end);
    buffer.resize(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(&buffer[0], buffer.size());

    unsigned int uncompressedSize;
    memcpy(&uncompressedSize, buffer.data(), sizeof(unsigned int));

    data = new char[uncompressedSize];

    const int decompressed_size = LZ4_decompress_safe(buffer.data() + sizeof(unsigned int), data, buffer.size() - sizeof(unsigned int), uncompressedSize);

    DWORD magicNumber;
    memcpy(&magicNumber, data, sizeof(DWORD));

    if (magicNumber != DDS_MAGIC) return false;

    return true;
}


} // raekor

