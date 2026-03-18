#include "pch.h"
#include "HammerComponent.h"
#include "Mesh.h"

IMPLEMENT_CLASS(UHammerComponent, UPrimitiveComponent)

UHammerComponent::UHammerComponent()
{
    Type = "Hammer";
}

void* UHammerComponent::operator new(size_t size)
{
    UE_LOG("Create : HammerComponent");
    UE_LOG("AllocationByte : %d", sizeof(UHammerComponent));

    return ::operator new(size);
}

void UHammerComponent::operator delete(void* ptr) noexcept
{
    return ::operator delete(ptr);
}

void UHammerComponent::Render(ID3D11DeviceContext& DeviceContext)
{
        MeshData->Draw(DeviceContext);
}