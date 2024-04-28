#pragma once

namespace Raekor {

class DXShader
{
public:
    enum Type
    {
        VERTEX, FRAG, GEO, COMPUTE
    };

    inline static auto sTypeTargets = std::array {
        "vs_5_0", "ps_5_0", "gs_5_0", "cs_5_0"
    };

    struct Stage
    {
        Type type;
        std::string mTextFile;
        fs::file_time_type mUpdateTime;
    };


public:
    DXShader() = default;
    DXShader(const Stage* stages, size_t stageCount);
    void Bind(ID3D11DeviceContext* inContext);

    void CheckForReload();
    bool CompileStage(const Stage& inStage);

private:
    Array<Stage> m_Stages;
    ComPtr<ID3D11VertexShader> m_VertexShader;
    ComPtr<ID3D11PixelShader> m_PixelShader;
    ComPtr<ID3D11ComputeShader> m_ComputeShader;
    ComPtr<ID3D11GeometryShader> m_GeometryShader;
};

} // namespace Raekor