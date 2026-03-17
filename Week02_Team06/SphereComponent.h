#pragma once

#include "PrimitiveComponent.h"

class UMesh;

class USphereComponent : public UPrimitiveComponent
{
    DECLARE_CLASS(USphereComponent, UPrimitiveComponent)

public:
    USphereComponent() = default;
    virtual ~USphereComponent() = default;

public:
    //new, delete 오버로딩
    void* operator new(size_t size);
    void operator delete(void* ptr) noexcept;

public:
    void TickComponent(float DeltaTime) override {};

    virtual void Render(ID3D11DeviceContext& DevcieContext) override;

};
