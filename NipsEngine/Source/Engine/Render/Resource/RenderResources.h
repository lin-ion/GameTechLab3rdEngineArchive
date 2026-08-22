#pragma once

/*
	Shader, Constant Buffer 등 렌더링에 필요한 리소스들을 관리하는 Class 입니다.
	Renderer에서 필요한 리소스들을 FRenderResources에 추가하여 관리할 수 있습니다.
*/

#include "Render/Resource/Shader.h"
#include "Render/Resource/Buffer.h"

struct FRenderResources
{
	FConstantBuffer FrameBuffer;					// b0
    FConstantBuffer PerObjectConstantBuffer;        // b1
    FConstantBuffer GizmoPerObjectConstantBuffer;   // b2
    FConstantBuffer EditorConstantBuffer;           // b4
	FConstantBuffer OutlineConstantBuffer;          // b5
    FConstantBuffer StaticMeshConstantBuffer;       // b6
    FConstantBuffer FxaaConstantBuffer;				// b7
    FConstantBuffer DepthSceneConstantBuffer;		// b8
	FConstantBuffer DecalConstantBuffer;			// b9
	FConstantBuffer FireBallConstantBuffer;			// b10
	FConstantBuffer HeightFogConstantBuffer;		// b11
	FConstantBuffer ViewportInfoConstantBuffer;		// b12
	

	FShader PrimitiveShader;
	FShader BillboardShader;
    FShader GizmoShader;
    FShader EditorShader;
	FShader SelectionMaskShader;
	FShader OutlineShader;
    FShader StaticMeshShader;
    FShader DepthSceneShader;
	// Fast Approximate Anti Aliasing용 Shader
    FShader FxaaShader;
    FShader DecalShader;
	FShader FireBallShader;
	FShader HeightFogShader;

	TComPtr<ID3D11SamplerState> MeshSamplerState;

    // FXAA 에서 선형 샘플링을 위해 필요한 SamplerState
    TComPtr<ID3D11SamplerState> LinearSamplerState;
    TComPtr<ID3D11SamplerState> DecalSamplerState;
	TComPtr<ID3D11SamplerState> PointSamplerState;
};
