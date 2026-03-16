#include "pch.h"
#include "GizmoComponent.h"
#include "World.h"
#include "Actor.h"
#include "Mesh.h"

void UGizmoComponent::TickComponent(float DeltaTime)
{
	FEditorViewportClient* Viewport = GetOwner()->GetWorld()->ViewPort;

	// 1. 카메라 행렬 획득 (View * Projection)
	FMatrix ViewProj = Viewport->GetViewMatrix() * Viewport->GetProjectionMatrix();

	// 2. 역스케일링 계산
	float Distance = (Viewport->GetViewLocation() - GetComponentLocation()).Length();
	float ScaleFactor = Distance * 0.15f;

	// 3. 기즈모의 월드 행렬 생성
	// GetTransform 대신 SceneComponent에 정의된 GetComponentTransform() 사용
	SetScale({ ScaleFactor, ScaleFactor, ScaleFactor });

}

void UGizmoComponent::Render(ID3D11DeviceContext& Context)
{
	MeshData->Draw(Context);
}

EGizmoAxis UGizmoComponent::CheckGizmoPicking(UPickingComponent* Picker)
{
/*	Picker->Is
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
	}*/

	return EGizmoAxis::None;
}
