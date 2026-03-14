#include "pch.h"
#include "MeshComponent.h"
#include "Mesh.h"
#include <d3d11.h>

void UMeshComponent::Release()
{
    // 내가 직접 만든 메쉬라면 파괴 공정 수행
    if (bIsMeshOwner && MeshAsset)
    {
        MeshAsset->Release();
        delete MeshAsset;
        MeshAsset = nullptr;
    }
    UPrimitiveComponent::Release();
}

void UMeshComponent::AddMesh(ID3D11Device& Device, const FVertexSimple* Vertices, UINT VertexCount)
{
    MeshAsset = new UMesh;
    MeshAsset->Load(Device, Vertices, VertexCount, nullptr, 0);
    bIsMeshOwner = true;
}

void UMeshComponent::Render(ID3D11DeviceContext* DeviceContext, const FMatrix& ViewProjection, ID3D11Buffer* ConstantBuffer)
{
    // 이제 여기서 MeshAsset이 nullptr이라 튕기는 일이 없습니다!
    if (!MeshAsset || !DeviceContext || !ConstantBuffer) return;

    struct FConstantData
    {
        //FMatrix Model;
        //FMatrix View;
        //FMatrix Projection;
        FMatrix MVP;

    };
    MeshAsset->BindBuffers(*DeviceContext);
    FMatrix Model = GetComponentTransform();
    FMatrix MVP = Model * ViewProjection;

    D3D11_MAPPED_SUBRESOURCE MSR;
    if (SUCCEEDED(DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MSR)))
    {
        memcpy(MSR.pData, &MVP, sizeof(FConstantData));
        DeviceContext->Unmap(ConstantBuffer, 0);
    }

    DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);

    if (MeshAsset->HasIndexBuffer())
    {
        DeviceContext->DrawIndexed(MeshAsset->GetIndexCount(), 0, 0);
    }
    else
    {
        DeviceContext->Draw(MeshAsset->GetVertexCount(), 0);
    }
}