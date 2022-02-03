#pragma once

namespace Raekor {

enum class ShaderType {
    FLOAT, FLOAT2, FLOAT3, FLOAT4,
    INT, INT2, INT3, INT4
};



constexpr uint32_t size_of(ShaderType type) {
    switch (type) {
        case ShaderType::INT:       return sizeof(int);
        case ShaderType::INT2:      return sizeof(int) * 2;
        case ShaderType::INT3:      return sizeof(int) * 3;
        case ShaderType::INT4:      return sizeof(int) * 4;
        case ShaderType::FLOAT:     return sizeof(float);
        case ShaderType::FLOAT2:    return sizeof(float) * 2;
        case ShaderType::FLOAT3:    return sizeof(float) * 3;
        case ShaderType::FLOAT4:    return sizeof(float) * 4;
        default: return 0;
    }
}



struct glShaderType {
    GLenum glType;
    uint8_t count;

    glShaderType(ShaderType shaderType);
};



struct Attribute {
    std::string name;
    ShaderType type;
    uint32_t size;
    uint32_t offset;

    Attribute(const std::string & pName, ShaderType type) : 
        name(pName), type(type), size(size_of(type)), offset(0) {}
};



class glVertexLayout {
public:
    glVertexLayout& attribute(const std::string& name, ShaderType type);
    
    inline size_t size() const { return layout.size(); }
    inline uint64_t getStride() const { return stride; }

    inline const auto begin() const { return layout.begin(); }
    inline const auto end() const { return layout.end(); }

    void bind();

private:
    uint32_t stride = 0;
    std::vector<Attribute> layout;
};

} // namespace Raekor