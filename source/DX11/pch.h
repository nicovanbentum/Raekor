#pragma once

#include "Raekor/pch.h"
#include "Raekor/raekor.h"

#include "imgui_impl_dx11.h"

#include <d3d11_1.h>
#include <wrl.h>
#include <dxgi.h>
#include <dxgi1_6.h>
#include <shellapi.h>
#include <d3dcompiler.h>

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

#define m_assert(expr, msg) assert(expr && msg)