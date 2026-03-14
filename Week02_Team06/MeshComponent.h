#pragma once
#include "PrimitiveComponent.h"

class UMesh;
struct ID3D11DeviceContext;
struct FVertexSimple;

class UMeshComponent : public UPrimitiveComponent
{
public:
    UMeshComponent() = default;
    virtual ~UMeshComponent() = default;

    // 자원을 할당했으면 여기서 치워야 합니다.
    void Release() override;

    // 외부에서 만들어진 메쉬를 연결할 때
    void SetMesh(UMesh* InMesh) { MeshAsset = InMesh; }

    // 직접 메쉬를 생성해서 가지고 있을 때 (이사 온 로직)
    void AddMesh(ID3D11Device& Device, const FVertexSimple* Vertices, UINT VertexCount);

    virtual void Render(ID3D11DeviceContext* DeviceContext, const FMatrix& ViewProjection, ID3D11Buffer* ConstantBuffer) override;

private:
    UMesh* MeshAsset = nullptr;
    bool bIsMeshOwner = false; // AddMesh로 직접 만들었을 때 메모리 해제를 위한 플래그
};