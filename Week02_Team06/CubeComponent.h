#pragma once
#include "PrimitiveComponent.h"

class UMesh;
struct FVertexSimple;

class UCubeComponent : public UPrimitiveComponent
{
public:
    UCubeComponent() = default;
    virtual ~UCubeComponent() = default;


public:
    /*Mesh는 단순 데이터*/
    UMesh* GetMesh() { return MeshAsset;  }
    void SetMesh(UMesh* InMesh) { MeshAsset = InMesh; }

    virtual void Render(ID3D11DeviceContext& DevcieContext) override;

private:
    UMesh* MeshAsset = nullptr;
    bool bIsMeshOwner = false; // AddMesh로 직접 만들었을 때 메모리 해제를 위한 플래그
};