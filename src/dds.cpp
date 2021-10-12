#include "pch.h"
#include "dds.h"

// Based on original by fabian "ryg" giesen v1.04
// Custom version, modified by Yann Collet
//
/*
    BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)
    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:
        Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
        Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
    You can contact the author at :
    - RygsDXTc source repository : http://code.google.com/p/rygsdxtc/
*/

namespace Raekor
{

void extractBlockFast(const unsigned char* src, int x, int y,
    int w, int h, unsigned char* block) {
    if ((w - x >= 4) && (h - y >= 4)) {
        // Full Square shortcut
        src += x * 4;
        src += y * w * 4;
        for (int i = 0; i < 4; ++i) {
            *(unsigned int*)block = *(unsigned int*)src; block += 4; src += 4;
            *(unsigned int*)block = *(unsigned int*)src; block += 4; src += 4;
            *(unsigned int*)block = *(unsigned int*)src; block += 4; src += 4;
            *(unsigned int*)block = *(unsigned int*)src; block += 4;
            src += (w * 4) - 12;
        }
        return;
    }
}

void extractBlock(const unsigned char* src, int x, int y,
    int w, int h, unsigned char* block) {
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
}

void rygCompress(unsigned char* dst, unsigned char* src, int w, int h, int isDxt5) {
    int x, y;
    unsigned char block[64];
    
    for (y = 0; y < h; y += 4) {
        for (x = 0; x < w; x += 4) {
            extractBlockFast(src, x, y, w, h, block);
            stb_compress_dxt_block(dst, block, isDxt5, 10);
            dst += isDxt5 ? 16 : 8;
        }
    }
}

} // raekor