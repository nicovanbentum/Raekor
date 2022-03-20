#pragma once

#include "Raekor/pch.h"

#include <dxgi1_6.h>
#include <d3dcompiler.h>

// DirectX-Headers helper library
#include "directx/d3dx12.h"

// AGILITY SDK Version 600
#include "Agility-SDK/d3d12.h"
#include "Agility-SDK/d3d12sdklayers.h"
#include "Agility-SDK/d3dcommon.h"
#include "Agility-SDK/dxgiformat.h"

// DIRECTX Compiler 6_6 Supported
#include "dxc/dxcapi.h"
#include "dxc/d3d12shader.h"

// DIRECTX Memory Allocator
#include "D3D12MemAlloc.h"

// DirectStorage Library
#include "dstorage.h"
#include "dstorageerr.h"

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
