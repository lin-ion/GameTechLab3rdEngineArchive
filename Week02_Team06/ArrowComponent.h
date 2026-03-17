#pragma once
#include "PrimitiveComponent.h"

class UArrowComponent : public UPrimitiveComponent
{
    DECLARE_CLASS(UArrowComponent, UPrimitiveComponent)

public:
    UArrowComponent() = default;
    virtual ~UArrowComponent() = default;

public:
    //new, delete 오버로딩
    void* operator new(size_t size);
    void operator delete(void* ptr) noexcept;

public:
    void TickComponent(float DeltaTime) override {};
    virtual void Render(ID3D11DeviceContext& DevcieContext) override;


};

