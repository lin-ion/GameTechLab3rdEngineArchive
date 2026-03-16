#pragma once

#include "pch.h"
#include "PrimitiveComponent.h"
#include "PickingComponent.h"
#include "App.h"
#include "ResourceManager.h"
#include "FEditorViewportClient.h"
#include "Renderer.h"
#include "Math.h"

enum class EGizmoAxis { None, Center, X, Y, Z };

class UGizmoComponent : public UPrimitiveComponent
{
public:
    void TickComponent(float DeltaTime) override;
    virtual void Render(ID3D11DeviceContext& Context) override;

public:

    EGizmoAxis CheckGizmoPicking(UPickingComponent* Picker);

};