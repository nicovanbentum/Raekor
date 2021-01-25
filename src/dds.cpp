#include "pch.h"
#include "dds.h"

namespace Raekor {

void compressDXT(unsigned char* dst, unsigned char* src, int w, int h, int isDxt5) {
    const int rem[] =
    {
       0, 0, 0, 0,
       0, 1, 0, 1,
       0, 1, 2, 0,
       0, 1, 2, 3
    };

    unsigned char block[64];
    int x, y;

    for (y = 0; y < h; y += 4) {
        for (x = 0; x < w; x += 4) {
            int i, j;

            int bw = std::min(w - x, 4);
            int bh = std::min(h - y, 4);
            int bx, by;

            const int rem[] =
            {
               0, 0, 0, 0,
               0, 1, 0, 1,
               0, 1, 2, 0,
               0, 1, 2, 3
            };

            for (i = 0; i < 4; ++i) {
                by = rem[(bh - 1) * 4 + i] + y;
                for (j = 0; j < 4; ++j) {
                    bx = rem[(bw - 1) * 4 + j] + x;
                    block[(i * 4 * 4) + (j * 4) + 0] =
                        src[(by * (w * 4)) + (bx * 4) + 0];
                    block[(i * 4 * 4) + (j * 4) + 1] =
                        src[(by * (w * 4)) + (bx * 4) + 1];
                    block[(i * 4 * 4) + (j * 4) + 2] =
                        src[(by * (w * 4)) + (bx * 4) + 2];
                    block[(i * 4 * 4) + (j * 4) + 3] =
                        src[(by * (w * 4)) + (bx * 4) + 3];
                }
            }

            stb_compress_dxt_block(dst, block, isDxt5, 10);
            dst += isDxt5 ? 16 : 8;
        }
    }
}

} // raekor