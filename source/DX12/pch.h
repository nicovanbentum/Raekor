#pragma once

#include "Raekor/pch.h"

#include "directx/d3dx12.h"
#include <dxgi1_6.h>
#include <d3dcompiler.h>

// AGILITY SDK VERSION 600
#include "Agility-SDK/d3d12.h"
#include "Agility-SDK/d3d12sdklayers.h"
#include "Agility-SDK/d3dcommon.h"
#include "Agility-SDK/dxgiformat.h"

// DIRECTX COMPILER 6_6 SUPPORTED
#include "dxc/dxcapi.h"
#include "dxc/d3d12shader.h"

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
