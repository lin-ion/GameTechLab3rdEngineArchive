#pragma once
#include <windows.h>
#include <assert.h>

#pragma region STL_CONTAINER
#include <unordered_map>
#include <array>
#include <string>
#include <cmath>
#include <vector>
#pragma endregion STL_CONTAINER

#pragma region DIERCT3D11
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#include <d3d11.h>
#include <d3dcompiler.h>
#pragma endregion D3D11_LIB

#include "Defines.h"
#include "Math.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "AppConsole.h"
