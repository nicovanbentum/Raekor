#pragma once

#include "Raekor/pch.h"
#include "Raekor/raekor.h"

// DirectX-Headers helper library
#include "directx/d3dx12.h"

#include <dxgi.h>
#include <dxgi1_6.h>
#include <shellapi.h>
#include <d3dcompiler.h>

// AGILITY SDK Version 600
#include "d3d12.h"
#include "d3d12sdklayers.h"
#include "d3dcommon.h"
#include "dxgiformat.h"

// DIRECTX Compiler 6_6 Supported
#include "dxc/dxcapi.h"
#include "dxc/d3d12shader.h"

// DIRECTX Memory Allocator
#include "D3D12MemAlloc.h"

// DirectStorage Library
#include "dstorage.h"
#include "dstorageerr.h"

// ImGui DX12 backend
#include "imgui_impl_dx12.h"

// AMD Fidelity-FX Super Resolution 2.1
#include "ffx_fsr2.h"
#include "dx12/ffx_fsr2_dx12.h"

// PIX Runtime Events
#include "pix3.h"

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
