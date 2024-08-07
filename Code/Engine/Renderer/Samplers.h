#pragma once

namespace RK::DX12 {

enum ESamplerIndex
{
    POINT_WRAP,
    MIN_MAG_MIP_POINT_WRAP,
    MIN_MAG_MIP_POINT_CLAMP,
    MIN_MAG_MIP_LINEAR_WRAP,
    MIN_MAG_MIP_LINEAR_CLAMP,
    ANISOTROPIC_WRAP,
    ANISOTROPIC_CLAMP,
    SAMPLER_COUNT,
};

constexpr std::array<D3D12_STATIC_SAMPLER_DESC, ESamplerIndex::SAMPLER_COUNT> STATIC_SAMPLER_DESC = {
    D3D12_STATIC_SAMPLER_DESC
    {
        D3D12_FILTER_MIN_MAG_MIP_POINT,  D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        0.0f, D3D12_MAX_MAXANISOTROPY, D3D12_COMPARISON_FUNC_NEVER, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK, 0.0f, 1.0f, POINT_WRAP, 0,
        D3D12_SHADER_VISIBILITY_ALL
    },

    D3D12_STATIC_SAMPLER_DESC
    {
        D3D12_FILTER_MIN_MAG_MIP_POINT,  D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        0.0f, D3D12_MAX_MAXANISOTROPY, D3D12_COMPARISON_FUNC_NEVER, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK, 0.0f, D3D12_FLOAT32_MAX, MIN_MAG_MIP_POINT_WRAP, 0,
        D3D12_SHADER_VISIBILITY_ALL
    },

    D3D12_STATIC_SAMPLER_DESC
    {
        D3D12_FILTER_MIN_MAG_MIP_POINT,  D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        0.0f, D3D12_MAX_MAXANISOTROPY, D3D12_COMPARISON_FUNC_NEVER, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK, 0.0f, D3D12_FLOAT32_MAX, MIN_MAG_MIP_POINT_CLAMP, 0,
        D3D12_SHADER_VISIBILITY_ALL
    },

    D3D12_STATIC_SAMPLER_DESC
    {
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,  D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        0.0f, D3D12_MAX_MAXANISOTROPY, D3D12_COMPARISON_FUNC_ALWAYS, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK, 0.0f, D3D12_FLOAT32_MAX, MIN_MAG_MIP_LINEAR_WRAP, 0,
        D3D12_SHADER_VISIBILITY_ALL
    },

    D3D12_STATIC_SAMPLER_DESC
    {
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,  D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        0.0f, D3D12_MAX_MAXANISOTROPY, D3D12_COMPARISON_FUNC_ALWAYS, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK, 0.0f, D3D12_FLOAT32_MAX, MIN_MAG_MIP_LINEAR_CLAMP, 0,
        D3D12_SHADER_VISIBILITY_ALL
    },

    D3D12_STATIC_SAMPLER_DESC
    {
        D3D12_FILTER_ANISOTROPIC,  D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        0.0f, D3D12_MAX_MAXANISOTROPY, D3D12_COMPARISON_FUNC_ALWAYS, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK, 0.0f, D3D12_FLOAT32_MAX, ANISOTROPIC_WRAP, 0,
        D3D12_SHADER_VISIBILITY_ALL
    },

    D3D12_STATIC_SAMPLER_DESC
    {
        D3D12_FILTER_ANISOTROPIC,  D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        0.0f, D3D12_MAX_MAXANISOTROPY, D3D12_COMPARISON_FUNC_NEVER, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK, 0.0f, D3D12_FLOAT32_MAX, ANISOTROPIC_CLAMP, 0,
        D3D12_SHADER_VISIBILITY_ALL
    }
};


constexpr std::array<D3D12_SAMPLER_DESC, ESamplerIndex::SAMPLER_COUNT> SAMPLER_DESC = {
    D3D12_SAMPLER_DESC
    {
        D3D12_FILTER_MIN_MAG_MIP_POINT,  D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        0.0f, D3D12_MAX_MAXANISOTROPY, D3D12_COMPARISON_FUNC_NEVER, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK, 0.0f, 1.0f,
        D3D12_SHADER_VISIBILITY_ALL
    },

    D3D12_SAMPLER_DESC
    {
        D3D12_FILTER_MIN_MAG_MIP_POINT,  D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        0.0f, D3D12_MAX_MAXANISOTROPY, D3D12_COMPARISON_FUNC_NEVER, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK, 0.0f, D3D12_FLOAT32_MAX,
        D3D12_SHADER_VISIBILITY_ALL
    },

    D3D12_SAMPLER_DESC
    {
        D3D12_FILTER_MIN_MAG_MIP_POINT,  D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        0.0f, D3D12_MAX_MAXANISOTROPY, D3D12_COMPARISON_FUNC_NEVER, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK, 0.0f, D3D12_FLOAT32_MAX,
        D3D12_SHADER_VISIBILITY_ALL
    },

    D3D12_SAMPLER_DESC
    {
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,  D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        0.0f, D3D12_MAX_MAXANISOTROPY, D3D12_COMPARISON_FUNC_NEVER, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK, 0.0f, D3D12_FLOAT32_MAX,
        D3D12_SHADER_VISIBILITY_ALL
    },

    D3D12_SAMPLER_DESC
    {
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,  D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        0.0f, D3D12_MAX_MAXANISOTROPY, D3D12_COMPARISON_FUNC_NEVER, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK, 0.0f, D3D12_FLOAT32_MAX,
        D3D12_SHADER_VISIBILITY_ALL
    },

    D3D12_SAMPLER_DESC
    {
        D3D12_FILTER_ANISOTROPIC,  D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        0.0f, D3D12_MAX_MAXANISOTROPY, D3D12_COMPARISON_FUNC_NEVER, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK, 0.0f, D3D12_FLOAT32_MAX,
        D3D12_SHADER_VISIBILITY_ALL
    },

    D3D12_SAMPLER_DESC
    {
        D3D12_FILTER_ANISOTROPIC,  D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        0.0f, D3D12_MAX_MAXANISOTROPY, D3D12_COMPARISON_FUNC_NEVER, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK, 0.0f, D3D12_FLOAT32_MAX,
        D3D12_SHADER_VISIBILITY_ALL
    }
};

}
