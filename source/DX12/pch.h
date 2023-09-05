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

// Nvidia DLSS 3.1
#include "nvsdk_ngx.h"
#include "nvsdk_ngx_defs.h"
#include "nvsdk_ngx_params.h"
#include "nvsdk_ngx_helpers.h"

// Intel XeSS
#include "xess.h"
#include "xess_debug.h"
#include "xess_d3d12.h"
#include "xess_d3d12_debug.h"

// PIX Runtime Events
#include "pix3.h"

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
