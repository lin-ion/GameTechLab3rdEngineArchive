#include "pch.h"
#include "UArrowComponent.h"
#include "Mesh.h"


IMPLEMENT_CLASS(UArrowComponent, UPrimitiveComponent)


void* UArrowComponent::operator new(size_t size)
{
    UE_LOG("Create : ArrowComponent");
    UE_LOG("AllocationByte : %d", sizeof(UArrowComponent));

    return ::operator new(size);
}

void UArrowComponent::operator delete(void* ptr) noexcept
{
    return ::operator delete(ptr);
}

void UArrowComponent::Render(ID3D11DeviceContext& DevcieContext)
{
    MeshData->Draw(DevcieContext);
}

