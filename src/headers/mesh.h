#pragma once

#include "buffer.h"

namespace Raekor {

constexpr std::array<Vertex, 8> unitCubeVertices = {
    Vertex{{-0.5f, -0.5f,  0.5f}},
    Vertex{{ 0.5f, -0.5f,  0.5f}},
    Vertex{{ 0.5f,  0.5f,  0.5f}},
    Vertex{{-0.5f,  0.5f,  0.5f}},
    Vertex{{-0.5f, -0.5f, -0.5f}},
    Vertex{{ 0.5f, -0.5f, -0.5f}},
    Vertex{{ 0.5f,  0.5f, -0.5f}},
    Vertex{{-0.5f,  0.5f, -0.5f}}
};



constexpr std::array<Triangle, 12>  cubeIndices = {
    Triangle{0, 1, 2}, Triangle{2, 3, 0},
    Triangle{1, 5, 6}, Triangle{6, 2, 1},
    Triangle{7, 6, 5}, Triangle{5, 4, 7},
    Triangle{4, 0, 3}, Triangle{3, 7, 4},
    Triangle{4, 5, 1}, Triangle{1, 0, 4},
    Triangle{3, 2, 6}, Triangle{6, 7, 3}
};



constexpr std::array<Vertex, 4> quadVertices = {
    Vertex{{-1.0f, -1.0f, 0.0f},  {0.0f, 0.0f}, {}, {}, {}},
    Vertex{{ 1.0f, -1.0f, 0.0f},  {1.0f, 0.0f}, {}, {}, {}},
    Vertex{{ 1.0f,  1.0f, 0.0f},  {1.0f, 1.0f}, {}, {}, {}},
    Vertex{{-1.0f,  1.0f, 0.0f},  {0.0f, 1.0f}, {}, {}, {}}
};



constexpr std::array<Triangle, 2> quadIndices = {
    Triangle{0, 1, 3}, Triangle{3, 1, 2}
};



constexpr std::array<Vertex, 4> planeVertices = {
    Vertex{{-0.5f, 0.0f, -0.5f}, {0.0f, 0.0f}, {0.0, 1.0, 0.0}, {}, {}},
    Vertex{{ 0.5f, 0.0f, -0.5f}, {1.0f, 0.0f}, {0.0, 1.0, 0.0}, {}, {}},
    Vertex{{ 0.5f, 0.0f,  0.5f}, {1.0f, 1.0f}, {0.0, 1.0, 0.0}, {}, {}},
    Vertex{{-0.5f, 0.0f,  0.5f}, {0.0f, 1.0f}, {0.0, 1.0, 0.0}, {}, {}}
};



constexpr std::array<Triangle, 2> planeIndices = {
    Triangle{3, 1, 0}, Triangle{2, 1, 3}
};

}