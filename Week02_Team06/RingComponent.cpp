#include "pch.h"
#include "RingComponent.h"
#include "Mesh.h"

IMPLEMENT_CLASS(URingComponent, UPrimitiveComponent)

void* URingComponent::operator new(size_t size)
{
    UE_LOG("Create : RingComponent");
    UE_LOG("AllocationByte : %d", sizeof(URingComponent));

    return ::operator new(size);
}

void URingComponent::operator delete(void* ptr) noexcept
{
    return ::operator delete(ptr);
}

void URingComponent::Render(ID3D11DeviceContext& DeviceContext)
{
    if (MeshData)
    {
        MeshData->Draw(DeviceContext);
    }
}