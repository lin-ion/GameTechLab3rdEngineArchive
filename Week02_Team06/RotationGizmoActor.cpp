#include "pch.h"
#include "RotationGizmoActor.h"
#include "World.h"
#include "RingComponent.h"
#include "FEditorViewportClient.h"
#include "ResourceManager.h"

IMPLEMENT_CLASS(URotationGizmoActor, AActor)

void URotationGizmoActor::BeginPlay()
{
	UWorld* World = GetWorld();
	UMesh* RotationMesh = World->resourceManager->FindMeshData("GizmoRotation");

	// 1. X축 회전 링 (Red) - YZ 평면에 배치되도록 회전
	RingX = AddComponent<URingComponent>();
	RingX->SetMesh(RotationMesh);
	RingX->SetRotation({ 0.0f, 0.0f, -90.0f }); // 필요에 따라 수정
	RingX->SetColor(ColorX);
	RingX->SetAlwaysOnTop(true);

	// 2. Y축 회전 링 (Green) - XZ 평면에 배치되도록 회전
	RingY = AddComponent<URingComponent>();
	RingY->SetMesh(RotationMesh);
	RingY->SetRotation({ 0.0f, 0.0f, 0.0f }); // 필요에 따라 수정
	RingY->SetColor(ColorY);
	RingY->SetAlwaysOnTop(true);

	// 3. Z축 회전 링 (Blue) - XY 평면에 배치 (기본형)
	RingZ = AddComponent<URingComponent>();
	RingZ->SetMesh(RotationMesh);
	RingZ->SetRotation({ 90.0f, 0.0f, 0.0f });
	RingZ->SetColor(ColorZ);
	RingZ->SetAlwaysOnTop(true);

	RootComponent = RingZ;
}

void URotationGizmoActor::Tick(float DeltaTime)
{
	if (!RootComponent) return;

	FVector CurrentPos = RootComponent->GetPosition();
	RingX->SetPosition(CurrentPos);
	RingY->SetPosition(CurrentPos);
	RingZ->SetPosition(CurrentPos);

	// 카메라 거리에 따른 스케일 보정
	FEditorViewportClient* Viewport = GetWorld()->ViewPort;
	float Distance = (Viewport->GetViewLocation() - RootComponent->GetComponentLocation()).Length();
	float ScaleFactor = Distance * 0.15f; // 메쉬 원본 크기에 따라 배율 조절 필요

	RingX->SetScale({ ScaleFactor, ScaleFactor, ScaleFactor });
	RingY->SetScale({ ScaleFactor, ScaleFactor, ScaleFactor });
	RingZ->SetScale({ ScaleFactor, ScaleFactor, ScaleFactor });

	RingX->SetColor(ColorX);
	RingY->SetColor(ColorY);
	RingZ->SetColor(ColorZ);

	// 호버링 및 드래그 상태에 따른 색상 하이라이트
	UWorld* World = GetWorld();
	EGizmoAxis ActiveAxis = (World != nullptr) ? World->GetDraggingAxis() : EGizmoAxis::None;

	if (ActiveAxis == EGizmoAxis::None)
	{
		ActiveAxis = CheckGizmoPicking();
	}

	switch (ActiveAxis)
	{
	case EGizmoAxis::X: RingX->SetColor(ColorHover); break;
	case EGizmoAxis::Y: RingY->SetColor(ColorHover); break;
	case EGizmoAxis::Z: RingZ->SetColor(ColorHover); break;
	case EGizmoAxis::None: break;
	}
}

EGizmoAxis URotationGizmoActor::CheckGizmoPicking()
{
	UWorld* World = GetWorld();
	if (!World || !World->ViewPort) return EGizmoAxis::None;

	FVector RayOrigin = World->ViewPort->GetViewLocation();
	FVector RayDir = World->ViewPort->GetCameraRayDirection();

	// 회전 링 피킹 검사
	if (World->RayIntersectsMesh(RayOrigin, RayDir, RingX->GetMesh(), RingX->GetComponentTransform())) return EGizmoAxis::X;
	if (World->RayIntersectsMesh(RayOrigin, RayDir, RingY->GetMesh(), RingY->GetComponentTransform())) return EGizmoAxis::Y;
	if (World->RayIntersectsMesh(RayOrigin, RayDir, RingZ->GetMesh(), RingZ->GetComponentTransform())) return EGizmoAxis::Z;

	return EGizmoAxis::None;
}

void URotationGizmoActor::Release()
{
	AActor::Release();
}