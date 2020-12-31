#pragma once

#include "buffer.h"

namespace Raekor {

constexpr std::array<Vertex, 8> unitCubeVertices = {
    Vertex{{0.0f, 0.0f, 0.0f}},
    Vertex{{1.0f, 0.0f, 0.0f}},
    Vertex{{1.0f, 1.0f, 0.0f}},
    Vertex{{0.0f, 1.0f, 0.0f}},
    Vertex{{0.0f, 0.0f, 1.0f}},
    Vertex{{1.0f, 0.0f, 1.0f}},
    Vertex{{1.0f, 1.0f, 1.0f}},
    Vertex{{0.0f, 1.0f, 1.0f}}
};

//////////////////////////////////////////////////////////////////////////////////////////////////

constexpr std::array<Triangle, 12>  cubeIndices = {
    Triangle{0, 1, 3}, Triangle{3, 1, 2},
    Triangle{1, 5, 2}, Triangle{2, 5, 6},
    Triangle{5, 4, 6}, Triangle{6, 4, 7},
    Triangle{4, 0, 7}, Triangle{7, 0, 3},
    Triangle{3, 2, 7}, Triangle{7, 2, 6},
    Triangle{4, 5, 0}, Triangle{0, 5, 1}
};

//////////////////////////////////////////////////////////////////////////////////////////////////

constexpr std::array<Vertex, 4> quadVertices = {
    Vertex{{-1.0f, -1.0f, 0.0f},  {0.0f, 0.0f},   {}, {}, {}},
    Vertex{{1.0f, -1.0f, 0.0f},   {1.0f, 0.0f},   {}, {}, {}},
    Vertex{{1.0f, 1.0f, 0.0f},    {1.0f, 1.0f},   {}, {}, {}},
    Vertex{{-1.0f, 1.0f, 0.0f},   {0.0f, 1.0f},   {}, {}, {}}
};

//////////////////////////////////////////////////////////////////////////////////////////////////

constexpr std::array<Triangle, 2> quadIndices = {
    Triangle{0, 1, 3}, Triangle{3, 1, 2}
};

//////////////////////////////////////////////////////////////////////////////////////////////////

constexpr std::array<Vertex, 4> planeVertices = {
    Vertex{{-1.0f, 0.0f, -1.0f},  {0.0f, 0.0f},   {0.0, 1.0, 0.0}, {}, {}},
    Vertex{{1.0f, 0.0f, -1.0f},   {1.0f, 0.0f},   {0.0, 1.0, 0.0}, {}, {}},
    Vertex{{1.0f, 0.0f, 1.0f},    {1.0f, 1.0f},   {0.0, 1.0, 0.0}, {}, {}},
    Vertex{{-1.0f, 0.0f, 1.0f},   {0.0f, 1.0f},   {0.0, 1.0, 0.0}, {}, {}}
};

constexpr std::array<Triangle, 2> planeIndices = {
    Triangle{3, 1, 0}, Triangle{2, 1, 3}
};

}