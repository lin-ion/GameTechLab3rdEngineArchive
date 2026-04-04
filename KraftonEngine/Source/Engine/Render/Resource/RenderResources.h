#pragma once
#include "Render/Resource/Buffer.h"
#include "Render/Pipeline/RenderCommand.h"

/*
	공용 Constant Buffer를 관리하는 구조체입니다.
	모든 커맨드가 공통으로 사용하는 Frame/PerObject CB만 소유합니다.
	타입별 CB(Gizmo, Editor, Outline 등)는 FConstantBufferPool에서 관리됩니다.
*/

struct FRenderResources
{
	FConstantBuffer FrameBuffer;				// b0 — ECBSlot::Frame
	FConstantBuffer PerObjectConstantBuffer;	// b1 — ECBSlot::PerObject (비-Opaque 패스용 유지)
	ID3D11SamplerState* DefaultSampler = nullptr;	// s0 — Linear/Wrap

#ifdef FOR_COMPETITION
	// Large Constant Buffer (전체 오브젝트의 PerObject 데이터, Opaque 패스용)
	ID3D11Buffer* PerObjectLargeCB = nullptr;
	uint32 LargeCBCapacity = 0;  // 현재 할당된 최대 오브젝트 수

	void CreatePerObjectLargeCB(ID3D11Device* Device, uint32 MaxObjects);
	void UpdatePerObjectLargeCB(ID3D11DeviceContext* Context,
	                            const FPerObjectAligned* Data, uint32 Count);
	void ReleasePerObjectLargeCB();
#endif

	void Create(ID3D11Device* InDevice);
	void Release();
};
