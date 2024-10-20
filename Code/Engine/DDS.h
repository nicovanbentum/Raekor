#pragma once

namespace RK {

//
// Function by Yann Collet @ https://github.com/Cyan4973/RygsDXTc
//
void CompressDXT(unsigned char* dst, unsigned char* src, int w, int h, int isDxt5);


#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((uint32_t)(unsigned char)(ch0) | ((uint32_t)(unsigned char)(ch1) << 8) |       \
                ((uint32_t)(unsigned char)(ch2) << 16) | ((uint32_t)(unsigned char)(ch3) << 24))
#endif /* defined(MAKEFOURCC) */


enum EDDSFormat
{
	DDS_FORMAT_DX10 = MAKEFOURCC('D', 'X', '1', '0'),
	DDS_FORMAT_DXT1 = MAKEFOURCC('D', 'X', 'T', '1'),
	DDS_FORMAT_DXT2 = MAKEFOURCC('D', 'X', 'T', '2'),
	DDS_FORMAT_DXT3 = MAKEFOURCC('D', 'X', 'T', '3'),
	DDS_FORMAT_DXT4 = MAKEFOURCC('D', 'X', 'T', '4'),
	DDS_FORMAT_DXT5 = MAKEFOURCC('D', 'X', 'T', '5'),
	DDS_FORMAT_ATI1 = MAKEFOURCC('A', 'T', 'I', '1'),
	DDS_FORMAT_ATI2 = MAKEFOURCC('A', 'T', 'I', '2')
};

enum EDDSPixelFormatFlags
{
	DDPF_ALPHAPIXELS = 0x1,
	DDPF_ALPHA = 0x2,
	DDPF_FOURCC = 0x4,
	DDPF_RGB = 0x40,
	DDPF_YUV = 0x200,
	DDPF_LUMINANCE = 0x20000
};

constexpr unsigned long DDS_MAGIC = 0x20534444;

struct DDS_PIXELFORMAT
{
	unsigned long dwSize;
	unsigned long dwFlags;
	unsigned long dwFourCC;
	unsigned long dwRGBBitCount;
	unsigned long dwRBitMask;
	unsigned long dwGBitMask;
	unsigned long dwBBitMask;
	unsigned long dwABitMask;
};

enum DDS_FLAGS
{
	DDSD_CAPS = 0x1,
	DDSD_HEIGHT = 0x2,
	DDSD_WIDTH = 0x4,
	DDSD_PITCH = 0x8,
	DDSD_PIXELFORMAT = 0x1000,
	DDSD_MIPMAPCOUNT = 0x20000,
	DDSD_LINEARSIZE = 0x80000,
	DDSD_DEPTH = 0x800000
};

enum DDSCAPS
{
	DDSCAPS_COMPLEX = 0x8,
	DDSCAPS_MIPMAP = 0x400000,
	DDSCAPS_TEXTURE = 0x1000
};

enum DDSCAPS2
{
	DDSCAPS2_CUBEMAP = 0x200,
	DDSCAPS2_CUBEMAP_POSITIVEX = 0x400,
	DDSCAPS2_CUBEMAP_NEGATIVEX = 0x800,
	DDSCAPS2_CUBEMAP_POSITIVEY = 0x1000,
	DDSCAPS2_CUBEMAP_NEGATIVEY = 0x2000,
	DDSCAPS2_CUBEMAP_POSITIVEZ = 0x4000,
	DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x8000,
	DDSCAPS2_VOLUME = 0x200000
};

#define DDS_CUBEMAP_ALLFACES (DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEX | DDSCAPS2_CUBEMAP_NEGATIVEX |\
                                                 DDSCAPS2_CUBEMAP_POSITIVEY | DDSCAPS2_CUBEMAP_NEGATIVEY |\
                                                 DDSCAPS2_CUBEMAP_POSITIVEZ | DDSCAPS2_CUBEMAP_NEGATIVEZ )


struct DDS_HEADER
{
	unsigned long   dwSize = 124;
	unsigned long   dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
	unsigned long   dwHeight;
	unsigned long   dwWidth;
	unsigned long   dwPitchOrLinearSize;
	unsigned long   dwDepth;
	unsigned long   dwMipMapCount;
	unsigned long   dwReserved1[11];
	DDS_PIXELFORMAT ddspf;
	unsigned long   dwCaps = DDSCAPS_TEXTURE;
	unsigned long   dwCaps2;
	unsigned long   dwCaps3;
	unsigned long   dwCaps4;
	unsigned long   dwReserved2;
};

static_assert( sizeof(DDS_HEADER) == 124 );


struct DDS_HEADER_DXT10
{
	unsigned int dxgiFormat;
	unsigned int resourceDimension;
	unsigned int miscFlag;
	unsigned int arraySize;
	unsigned int miscFlags2;
};


struct DDSFileInfo
{
	uint32_t magicNumber;
	DDS_HEADER header;
};

static_assert( sizeof(DDSFileInfo) == 128 );


struct DDSFileInfoExtended
{
	uint32_t magicNumber;
	DDS_HEADER header;
	DDS_HEADER_DXT10 header10;
};


} // raekor
