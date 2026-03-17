#include "pch.h"
#include "CubeComponent.h"
#include "Mesh.h"

IMPLEMENT_CLASS(UCubeComponent, UPrimitiveComponent)

UCubeComponent::UCubeComponent()
{
    Type = "Cube";
}

void* UCubeComponent::operator new(size_t size)
{
    UE_LOG("Create : CubeComponent");
    UE_LOG("AllocationByte : %d", sizeof(UCubeComponent));

    return ::operator new(size);
}

void UCubeComponent::operator delete(void* ptr) noexcept
{
    return ::operator delete(ptr);
}

void UCubeComponent::Render(ID3D11DeviceContext& DeviceContext)
{
    MeshData->Draw(DeviceContext);
}