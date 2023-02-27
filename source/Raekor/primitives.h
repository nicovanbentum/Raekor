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


inline void gGenerateSphere(Mesh& ioMesh, float inRadius, uint32_t inSectorCount, uint32_t inStackCount) {
    ioMesh.positions.clear();
    ioMesh.normals.clear();
    ioMesh.tangents.clear();
    ioMesh.uvs.clear();

    float x, y, z, xy;                              // vertex position
    float nx, ny, nz, lengthInv = 1.0f / inRadius;  // vertex normal
    float s, t;                                     // vertex texCoord

    const float PI = static_cast<float>(M_PI);
    float sectorStep = 2 * PI / inSectorCount;
    float stackStep = PI / inStackCount;
    float sectorAngle, stackAngle;

    for (int i = 0; i <= inStackCount; ++i) {
        stackAngle = PI / 2 - i * stackStep;        // starting from pi/2 to -pi/2
        xy = inRadius * cosf(stackAngle);             // r * cos(u)
        z = inRadius * sinf(stackAngle);              // r * sin(u)

        // add (sectorCount+1) vertices per stack
        // the first and last vertices have same position and normal, but different tex coords
        for (int j = 0; j <= inSectorCount; ++j) {
            sectorAngle = j * sectorStep;           // starting from 0 to 2pi

            // vertex position (x, y, z)
            x = xy * cosf(sectorAngle);             // r * cos(u) * cos(v)
            y = xy * sinf(sectorAngle);             // r * cos(u) * sin(v)
            ioMesh.positions.emplace_back(x, y, z);

            // normalized vertex normal (nx, ny, nz)
            nx = x * lengthInv;
            ny = y * lengthInv;
            nz = z * lengthInv;
            ioMesh.normals.emplace_back(nx, ny, nz);

            // vertex tex coord (s, t) range between [0, 1]
            s = (float)j / inSectorCount;
            t = (float)i / inStackCount;
            ioMesh.uvs.emplace_back(s, t);

        }
    }

    int k1, k2;
    for (int i = 0; i < inStackCount; ++i) {
        k1 = i * (inSectorCount + 1);     // beginning of current stack
        k2 = k1 + inSectorCount + 1;      // beginning of next stack

        for (int j = 0; j < inSectorCount; ++j, ++k1, ++k2) {
            // 2 triangles per sector excluding first and last stacks
            // k1 => k2 => k1+1
            if (i != 0) {
                ioMesh.indices.push_back(k1);
                ioMesh.indices.push_back(k2);
                ioMesh.indices.push_back(k1 + 1);
            }

            // k1+1 => k2 => k2+1
            if (i != (inStackCount - 1)) {
                ioMesh.indices.push_back(k1 + 1);
                ioMesh.indices.push_back(k2);
                ioMesh.indices.push_back(k2 + 1);
            }
        }
    }

    ioMesh.CalculateTangents();
    ioMesh.CalculateAABB();
}

}