#include "pch.h"
#include "GizmoComponent.h"
#include "App.h"
#include "ResourceManager.h"
#include "FEditorViewportClient.h"
#include "Renderer.h"
#include "Math.h"
#include "PickingComponent.h"
#include "Actor.h"
#include "World.h"

IMPLEMENT_CLASS(UGizmoComponent, UPrimitiveComponent)

void UGizmoComponent::TickComponent(float DeltaTime)
{
	// 부모의 틱을 호출해 줍니다.
	UPrimitiveComponent::TickComponent(DeltaTime);

	FEditorViewportClient* Viewport = UApp::GetInstance().GetViewportClient();
	if (!Viewport) return;

	// 1. 카메라와의 거리를 계산하여 역스케일링(크기 유지)을 수행합니다.
	float Distance = (Viewport->GetViewLocation() - GetComponentLocation()).Length();
	float ScaleFactor = Distance * 0.15f;

	// SceneComponent의 기능을 이용해 스케일을 적용합니다. 
	// (이후 GetComponentTransform() 호출 시 이 스케일이 자동으로 반영됩니다.)
	SetScale({ ScaleFactor, ScaleFactor, ScaleFactor });
}

void UGizmoComponent::Render(ID3D11DeviceContext* DeviceContext, const FMatrix& ViewProjection, ID3D11Buffer* ConstantBuffer)
{
    if (!DeviceContext) return;

    UResourceManager* ResMgr = UApp::GetInstance().GetResourceManager();
    UMesh* ArrowMesh = ResMgr->FindMeshData("Gizmo");
    UMesh* SphereMesh = ResMgr->FindMeshData("Sphere");
    if (!ArrowMesh) return;

    URenderer* Renderer = UApp::GetInstance().GetRenderer();

    // TickComponent에서 이미 스케일이 적용된 변환 행렬을 그대로 가져옵니다.
    FMatrix BaseModel = GetComponentTransform();

    // 1. Center Sphere 렌더링 (화살표보다 10% 크기로 렌더링)
    if (SphereMesh)
    {
        // BaseModel에 이미 거리에 따른 스케일이 곱해져 있으므로, 구체의 비율(0.1)만 추가로 곱해줍니다.
        FMatrix SphereModel = FMatrix::MakeScale({ 0.1f, 0.1f, 0.1f }) * BaseModel;
        Renderer->UpdateConstantBuffer(*DeviceContext, SphereModel * ViewProjection, { 1.f, 1.f, 1.f, 1.f });
        SphereMesh->Draw(*DeviceContext);
    }

    // 2. Axis Arrows 렌더링
   if (ArrowMesh)
   {
       // Y축 (Green)
       Renderer->UpdateConstantBuffer(*DeviceContext, BaseModel * ViewProjection, { 0.f, 1.f, 0.f, 1.f });
       ArrowMesh->Draw(*DeviceContext);
   
       // X축 (Red - Z축 기준 -90도 회전)
       FMatrix XRotation = FMatrix::MakeRotationZ(-90.0f);
       Renderer->UpdateConstantBuffer(*DeviceContext, (XRotation * BaseModel) * ViewProjection, { 1.0f, 0.f, 0.f, 1.f });
       ArrowMesh->Draw(*DeviceContext);
   
       // Z축 (Blue - X축 기준 90도 회전)
       FMatrix ZRotation = FMatrix::MakeRotationX(90.0f);
       Renderer->UpdateConstantBuffer(*DeviceContext, (ZRotation * BaseModel) * ViewProjection, { 0.f, 0.f, 1.f, 1.f });
       ArrowMesh->Draw(*DeviceContext);
   }
}

EGizmoAxis UGizmoComponent::CheckGizmoPicking(UPickingComponent* Picker)
{
    if (!Picker) return EGizmoAxis::None;

    UResourceManager* ResMgr = UApp::GetInstance().GetResourceManager();
    UMesh* ArrowMesh = ResMgr->FindMeshData("Gizmo");

    FEditorViewportClient* Viewport = UApp::GetInstance().GetViewportClient();

    // TickComponent에서 이미 스케일이 조정되었으므로 그대로 사용합니다.
    FMatrix BaseModel = GetComponentTransform();

    // 각 축 화살표 검수 (렌더링 때와 동일한 회전 적용)
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
