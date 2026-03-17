#pragma once
#include "PrimitiveComponent.h"

class URingComponent : public UPrimitiveComponent
{
    DECLARE_CLASS(URingComponent, UPrimitiveComponent)

public:
    URingComponent() = default;
    virtual ~URingComponent() = default;

public:
    // new, delete 오버로딩
    void* operator new(size_t size);
    void operator delete(void* ptr) noexcept;

public:
    void TickComponent(float DeltaTime) override {};
    virtual void Render(ID3D11DeviceContext& DeviceContext) override;
};