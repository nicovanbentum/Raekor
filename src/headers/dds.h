#pragma once

namespace Raekor {
//
// Function by Yann Collet @ https://github.com/Cyan4973/RygsDXTc
//
void rygCompress(unsigned char* dst, unsigned char* src, int w, int h, int isDxt5);

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((uint32)(byte)(ch0) | ((uint32)(byte)(ch1) << 8) |       \
                ((uint32)(byte)(ch2) << 16) | ((uint32)(byte)(ch3) << 24))
#endif /* defined(MAKEFOURCC) */

constexpr DWORD DDS_MAGIC = 0x20534444;

struct DDS_PIXELFORMAT {
    DWORD dwSize = 32;
    DWORD dwFlags = 0x4;
    DWORD dwFourCC = MAKEFOURCC('D', 'X', 'T', '5');
    DWORD dwRGBBitCount = 32;
    DWORD dwRBitMask = 0xff000000;
    DWORD dwGBitMask = 0x00ff0000;
    DWORD dwBBitMask = 0x0000ff00;
    DWORD dwABitMask = 0x000000ff;
};

enum DDS_FLAGS {
    DDSD_CAPS = 0x1,
    DDSD_HEIGHT = 0x2,
    DDSD_WIDTH = 0x4,
    DDSD_PITCH	= 0x8,
    DDSD_PIXELFORMAT = 0x1000,
    DDSD_MIPMAPCOUNT = 0x20000,
    DDSD_LINEARSIZE	= 0x80000,
    DDSD_DEPTH = 0x800000
};

enum DDSCAPS {
    DDSCAPS_COMPLEX = 0x8,
    DDSCAPS_MIPMAP = 0x400000,
    DDSCAPS_TEXTURE = 0x1000
};

enum DDSCAPS2 {
    DDSCAPS2_CUBEMAP = 0x200,
    DDSCAPS2_CUBEMAP_POSITIVEX 	= 0x400,
    DDSCAPS2_CUBEMAP_NEGATIVEX 	= 0x800,
    DDSCAPS2_CUBEMAP_POSITIVEY 	= 0x1000,
    DDSCAPS2_CUBEMAP_NEGATIVEY 	= 0x2000,
    DDSCAPS2_CUBEMAP_POSITIVEZ 	= 0x4000,
    DDSCAPS2_CUBEMAP_NEGATIVEZ 	= 0x8000,
    DDSCAPS2_VOLUME = 0x200000
};

#define DDS_CUBEMAP_ALLFACES (DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEX | DDSCAPS2_CUBEMAP_NEGATIVEX |\
                                                 DDSCAPS2_CUBEMAP_POSITIVEY | DDSCAPS2_CUBEMAP_NEGATIVEY |\
                                                 DDSCAPS2_CUBEMAP_POSITIVEZ | DDSCAPS2_CUBEMAP_NEGATIVEZ )


struct DDS_HEADER {
    DWORD           dwSize = 124;
    DWORD           dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
    DWORD           dwHeight;
    DWORD           dwWidth;
    DWORD           dwPitchOrLinearSize;
    DWORD           dwDepth;
    DWORD           dwMipMapCount;
    DWORD           dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    DWORD           dwCaps = DDSCAPS_TEXTURE;
    DWORD           dwCaps2;
    DWORD           dwCaps3;
    DWORD           dwCaps4;
    DWORD           dwReserved2;
};

static_assert(sizeof(DDS_HEADER) == 124);

} // raekor
