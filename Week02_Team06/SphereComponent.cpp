#include "pch.h"
#include "SphereComponent.h"

#include "Mesh.h"

IMPLEMENT_CLASS(USphereComponent, UPrimitiveComponent)

USphereComponent::USphereComponent()
{
    Type = "Sphere";
}

void* USphereComponent::operator new(size_t size)
{
    UE_LOG("Create : SphereComponent");
    UE_LOG("AllocationByte : %d", sizeof(USphereComponent));

    return ::operator new(size);
}

void USphereComponent::operator delete(void* ptr) noexcept
{
    return ::operator delete(ptr);
}

void USphereComponent::Render(ID3D11DeviceContext& DeviceContext)
{
    MeshData->Draw(DeviceContext);
}