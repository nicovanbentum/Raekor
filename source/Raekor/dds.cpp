#include "pch.h"
#include "dds.h"

namespace Raekor {

void CompressDXT(unsigned char* dst, unsigned char* src, int w, int h, int isDxt5)
{
	int x, y;
	unsigned char block[64];

	auto ExtractBlock = [](const unsigned char* src, int x, int y, int w, int h, unsigned char* block)
	{
		if (( w - x >= 4 ) && ( h - y >= 4 ))
		{
			// Full Square shortcut
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

	for (y = 0; y < h; y += 4)
	{
		for (x = 0; x < w; x += 4)
		{
			ExtractBlock(src, x, y, w, h, block);
			stb_compress_dxt_block(dst, block, isDxt5, 10);
			dst += isDxt5 ? 16 : 8;
		}
	}
}

} // raekor