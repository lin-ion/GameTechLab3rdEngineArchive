#include "pch.h"
#include "GizmoActor.h"
#include "World.h"
#include "ArrowComponent.h"
#include "SphereComponent.h"

#include "FEditorViewportClient.h"

IMPLEMENT_CLASS(UGizmoActor, AActor)

void UGizmoActor::BeginPlay()
{
	ArrowY = AddComponent<UArrowComponent>();
	ArrowY->SetColor({ 0.f, 1.f, 0.f, 1.f });

	ArrowX = AddComponent<UArrowComponent>();
	ArrowX->SetRotation({ 0.0f, 0.0f, -90.0f });
	ArrowX->SetColor({ 1.f, 0.f, 0.f, 1.f });

	ArrowZ = AddComponent<UArrowComponent>();
	ArrowZ->SetRotation({ 90.0f, 0.0f, 0.0f });
	ArrowZ->SetColor({ 0.f, 0.f, 1.f, 1.f });

	BasePoint = AddComponent<USphereComponent>();
	BasePoint->SetColor({ 1.f, 1.f, 1.f, 1.f });

	// Sphere가 Base
	RootComponent = BasePoint;
}

void UGizmoActor::Tick(float DeltaTime)
{
	if (!RootComponent) return;

	FEditorViewportClient* Viewport = GetWorld()->ViewPort;

	float Distance = (Viewport->GetViewLocation() - RootComponent->GetComponentLocation()).Length();
	float ScaleFactor = Distance * 0.15f;

	ArrowY->SetScale({ ScaleFactor, ScaleFactor, ScaleFactor });
	ArrowX->SetScale({ ScaleFactor, ScaleFactor, ScaleFactor });
	ArrowZ->SetScale({ ScaleFactor, ScaleFactor, ScaleFactor });
	BasePoint->SetScale({ ScaleFactor * 0.1f, ScaleFactor * 0.1f, ScaleFactor * 0.1f });

	ArrowX->SetColor(ColorX);
	ArrowY->SetColor(ColorY);
	ArrowZ->SetColor(ColorZ);
	BasePoint->SetColor(ColorCenter);

	EGizmoAxis HoveredAxis = CheckGizmoPicking();

	switch (HoveredAxis)
	{
	case EGizmoAxis::X: ArrowX->SetColor(ColorHover); break;
	case EGizmoAxis::Y: ArrowY->SetColor(ColorHover); break;
	case EGizmoAxis::Z: ArrowZ->SetColor(ColorHover); break;
	case EGizmoAxis::Center: BasePoint->SetColor(ColorHover); break;
	case EGizmoAxis::None: break;
	}
}

EGizmoAxis UGizmoActor::CheckGizmoPicking()
{
	UWorld* World = GetWorld();
	if (!World || !World->ViewPort) return EGizmoAxis::None;

	FVector RayOrigin = World->ViewPort->GetViewLocation();
	FVector RayDir = World->ViewPort->GetCameraRayDirection();

	// 구체(Center)를 가장 먼저 검사 (제일 작고 중앙에 있으므로)
	if (World->RayIntersectsMesh(RayOrigin, RayDir, BasePoint->GetMesh(), BasePoint->GetComponentTransform()))
		return EGizmoAxis::Center;

	// 각 화살표 검사 (이미 회전이 적용된 상태이므로 그대로 넘김)
	if (World->RayIntersectsMesh(RayOrigin, RayDir, ArrowX->GetMesh(), ArrowX->GetComponentTransform())) return EGizmoAxis::X;
	if (World->RayIntersectsMesh(RayOrigin, RayDir, ArrowY->GetMesh(), ArrowY->GetComponentTransform())) return EGizmoAxis::Y;
	if (World->RayIntersectsMesh(RayOrigin, RayDir, ArrowZ->GetMesh(), ArrowZ->GetComponentTransform())) return EGizmoAxis::Z;

	return EGizmoAxis::None;
}

void UGizmoActor::Release()
{
	AActor::Release();
}
