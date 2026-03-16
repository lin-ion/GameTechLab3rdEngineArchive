#pragma once

#include "Defines.h"
#include "EngineStatics.h"
#include "Object.h"
#include "PrimitiveComponent.h"

class UApp;
class UResourceManager;
class FEditorViewportClient;
class UPickingComponent;
struct ID3D11DeviceContext;
struct ID3D11Buffer;

enum class EGizmoAxis { None, Center, X, Y, Z };

class UGizmoComponent : public UPrimitiveComponent
{
    DECLARE_CLASS(UGizmoComponent, UPrimitiveComponent)

public:
    UGizmoComponent() = default;
    virtual ~UGizmoComponent() = default;

public:
    // 렌더링 규격을 부모(USceneComponent)와 완벽히 일치시킵니다.
    virtual void Render(ID3D11DeviceContext* DeviceContext, const FMatrix& ViewProjection, ID3D11Buffer* ConstantBuffer) override;

    // 피킹 로직 선언
    EGizmoAxis CheckGizmoPicking(UPickingComponent* Picker);
};