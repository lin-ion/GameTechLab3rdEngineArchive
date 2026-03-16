#include "pch.h"
#include "GizmoComponent.h"
#include "App.h"
#include "ResourceManager.h"
#include "FEditorViewportClient.h"
#include "Renderer.h"
#include "Math.h"
#include "PickingComponent.h"

IMPLEMENT_CLASS(UGizmoComponent, UPrimitiveComponent)

void UGizmoComponent::Render(ID3D11DeviceContext* DeviceContext, const FMatrix& ViewProjection, ID3D11Buffer* ConstantBuffer)
{
    if (!DeviceContext || !GApp) return;

        UResourceManager* ResMgr = GApp->GetResourceManager();
    UMesh* ArrowMesh = ResMgr->FindMeshData("Gizmo");
    UMesh* SphereMesh = ResMgr->FindMeshData("Sphere");
    if (!ArrowMesh) return;

    FEditorViewportClient* Viewport = GApp->GetViewportClient();
    float Distance = (Viewport->GetViewLocation() - GetComponentLocation()).Length();
    float ScaleFactor = Distance * 0.15f;

    FMatrix BaseModel = FMatrix::MakeScale({ ScaleFactor, ScaleFactor, ScaleFactor }) * GetComponentTransform();
    URenderer* Renderer = GApp->GetRenderer();

    // 1. Center Sphere 렌더링
    if (SphereMesh)
    {
        FMatrix SphereModel = FMatrix::MakeScale({ ScaleFactor * 0.1f, ScaleFactor * 0.1f, ScaleFactor * 0.1f }) * GetComponentTransform();
        Renderer->UpdateConstantBuffer(*DeviceContext, SphereModel * ViewProjection, { 1.f, 1.f, 1.f, 1.f });
        SphereMesh->Draw(*DeviceContext);
    }

    // 2. Axis Arrows 렌더링
    if (ArrowMesh)
    {
        // Y축 (Green)
        Renderer->UpdateConstantBuffer(*DeviceContext, BaseModel * ViewProjection, { 0.f, 1.f, 0.f, 1.f });
        ArrowMesh->Draw(*DeviceContext);

        // X축 (Red)
        FMatrix XRotation = FMatrix::MakeRotationZ(-90.0f);
        Renderer->UpdateConstantBuffer(*DeviceContext, (XRotation * BaseModel) * ViewProjection, { 1.0f, 0.f, 0.f, 1.f });
        ArrowMesh->Draw(*DeviceContext);

        // Z축 (Blue)
        FMatrix ZRotation = FMatrix::MakeRotationX(90.0f);
        Renderer->UpdateConstantBuffer(*DeviceContext, (ZRotation * BaseModel) * ViewProjection, { 0.f, 0.f, 1.f, 1.f });
        ArrowMesh->Draw(*DeviceContext);
    }
}

EGizmoAxis UGizmoComponent::CheckGizmoPicking(UPickingComponent* Picker)
{
    if (!Picker) return EGizmoAxis::None;

    UResourceManager* ResMgr = GApp->GetResourceManager();
    UMesh* ArrowMesh = ResMgr->FindMeshData("Gizmo");
    UMesh* SphereMesh = ResMgr->FindMeshData("Sphere");

    FEditorViewportClient* Viewport = GApp->GetViewportClient();
    float Distance = (Viewport->GetViewLocation() - GetComponentLocation()).Length();
    float ScaleFactor = Distance * 0.15f;
    FMatrix BaseModel = FMatrix::MakeScale({ ScaleFactor, ScaleFactor, ScaleFactor }) * GetComponentTransform();

    // 2. 각 축 화살표 검수 (렌더링 때와 동일한 회전 적용)
    if (ArrowMesh)
    {
        // Y축 (Green - 기본)
        if (Picker->IsPicked(ArrowMesh, Viewport->GetViewLocation(), Viewport->GetCameraRayDirection(), BaseModel)) return EGizmoAxis::Y;

        // X축 (Red - Z축 -90도 회전)
        FMatrix XRotation = FMatrix::MakeRotationZ(-90.0f);
        if (Picker->IsPicked(ArrowMesh, Viewport->GetViewLocation(), Viewport->GetCameraRayDirection(), XRotation * BaseModel)) return EGizmoAxis::X;

        // Z축 (Blue - X축 90도 회전)
        FMatrix ZRotation = FMatrix::MakeRotationX(90.0f);
        if (Picker->IsPicked(ArrowMesh, Viewport->GetViewLocation(), Viewport->GetCameraRayDirection(), ZRotation * BaseModel)) return EGizmoAxis::Z;
    }

    return EGizmoAxis::None;
}