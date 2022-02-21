#pragma once

namespace Raekor {

struct Vertex {
    constexpr Vertex(
        glm::vec3 p = {}, 
        glm::vec2 uv = {}, 
        glm::vec3 n = {}, 
        glm::vec3 t = {}
    ) :
        pos(p), 
        uv(uv), 
        normal(n), 
        tangent(t) 
    {}

    glm::vec3 pos;
    glm::vec2 uv;
    glm::vec3 normal;
    glm::vec3 tangent;
};


struct Triangle {
    constexpr Triangle(
        uint32_t _p1 = {}, 
        uint32_t _p2 = {}, 
        uint32_t _p3 = {}
    ) :
        p1(_p1), 
        p2(_p2), 
        p3(_p3) 
    {}

    uint32_t p1, p2, p3;
};


struct UnitCube {
    static constexpr std::array<Vertex, 8> vertices = {
        Vertex{{-0.5f, -0.5f,  0.5f}},
        Vertex{{ 0.5f, -0.5f,  0.5f}},
        Vertex{{ 0.5f,  0.5f,  0.5f}},
        Vertex{{-0.5f,  0.5f,  0.5f}},
        Vertex{{-0.5f, -0.5f, -0.5f}},
        Vertex{{ 0.5f, -0.5f, -0.5f}},
        Vertex{{ 0.5f,  0.5f, -0.5f}},
        Vertex{{-0.5f,  0.5f, -0.5f}}
    };

    static constexpr std::array<Triangle, 12>  indices = {
        Triangle{0, 1, 2}, Triangle{2, 3, 0},
        Triangle{1, 5, 6}, Triangle{6, 2, 1},
        Triangle{7, 6, 5}, Triangle{5, 4, 7},
        Triangle{4, 0, 3}, Triangle{3, 7, 4},
        Triangle{4, 5, 1}, Triangle{1, 0, 4},
        Triangle{3, 2, 6}, Triangle{6, 7, 3}
    };
};


struct UnitPlane {
    static constexpr std::array<Vertex, 4> vertices = {
        Vertex{{-0.5f, 0.0f, -0.5f}, {0.0f, 0.0f}, {0.0, 1.0, 0.0}, {}},
        Vertex{{ 0.5f, 0.0f, -0.5f}, {1.0f, 0.0f}, {0.0, 1.0, 0.0}, {}},
        Vertex{{ 0.5f, 0.0f,  0.5f}, {1.0f, 1.0f}, {0.0, 1.0, 0.0}, {}},
        Vertex{{-0.5f, 0.0f,  0.5f}, {0.0f, 1.0f}, {0.0, 1.0, 0.0}, {}}
    };

    static constexpr std::array<Triangle, 2> indices = {
        Triangle{3, 1, 0}, Triangle{2, 1, 3}
    };
};

}