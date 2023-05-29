#pragma once

#include "pch.h"

namespace Raekor {




class DXShader {
public:
enum Type {
    VERTEX, FRAG, GEO, COMPUTE
};

inline static auto sTypeTargets = std::array {
    "vs_5_0", "ps_5_0", "gs_5_0", "cs_5_0"
};
    
struct Stage {
    Type type;
    std::string textfile;
    FileSystem::file_time_type updatetime;
};


public:
    DXShader() = default;
    DXShader(const Stage* stages, size_t stageCount);
    void Bind();

    void CheckForReload();
    bool CompileStage(const Stage& inStage);

private:
    std::vector<Stage> m_stages;
    ComPtr<ID3D11VertexShader> vertex_shader;
    ComPtr<ID3D11PixelShader> pixel_shader;
    ComPtr<ID3D11ComputeShader> compute_shader;
    ComPtr<ID3D11GeometryShader> geo_shader;
};

} // namespace Raekor