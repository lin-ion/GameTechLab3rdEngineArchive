#include "pch.h"
#include "GizmoActor.h"
#include "World.h"
#include "ArrowComponent.h"
#include "SphereComponent.h"
#include "FEditorViewportClient.h"

IMPLEMENT_CLASS(UGizmoActor, AActor)

void UGizmoActor::BeginPlay()
{
	AActor::BeginPlay();

	ArrowY = AddComponent<UArrowComponent>();
	ArrowY->SetColor({0.f, 1.f, 0.f, 1.f});
	ArrowY->SetRelativeScale({ 3.f, 3.f, 3.f });

	ArrowX = AddComponent<UArrowComponent>();
	ArrowX->SetRotation({ 0.0f, 0.0f, -90.0f });
	ArrowX->SetColor({1.f, 0.f, 0.f, 1.f});
	ArrowX->SetRelativeScale({ 3.f, 3.f, 3.f });

	ArrowZ = AddComponent<UArrowComponent>();
	ArrowZ->SetColor({ 0.f, 0.f, 1.f, 1.f });
	ArrowZ->SetRotation({ 90.0f, 0.0f, 0.0f });
	ArrowZ->SetRelativeScale({ 3.f, 3.f, 3.f });

	BasePoint = AddComponent<USphereComponent>();
	BasePoint->SetScale({ 0.5f, 0.5f, 0.5f });

	ArrowY->SetAlwaysOnTop(true);
	ArrowX->SetAlwaysOnTop(true);
	ArrowZ->SetAlwaysOnTop(true);
	BasePoint->SetAlwaysOnTop(true);

	// Sphere가 Base
	RootComponent = BasePoint;
}

void UGizmoActor::Tick(float DeltaTime)
{
	AActor::Tick(DeltaTime);
	FEditorViewportClient* Viewport = GetWorld()->ViewPort;

	float Distance = (Viewport->GetViewLocation() - RootComponent->GetComponentLocation()).Length();
	float ScaleFactor = Distance * 0.15f;
	FVector ResultScale = { ScaleFactor, ScaleFactor, ScaleFactor };

	ArrowY->SetScale(ResultScale);
	ArrowX->SetScale(ResultScale);
	ArrowZ->SetScale(ResultScale);
	BoundingSphere->SetScale(ResultScale);

	RootComponent->SetPosition(ArrowX->GetPosition() + ArrowX->GetForwardVector() * DeltaTime);
	// 일단 모두 기본 색상으로 초기화
	ArrowX->SetColor(ColorX);
	ArrowY->SetColor(ColorY);
	ArrowZ->SetColor(ColorZ);
	BasePoint->SetColor(ColorCenter);

	// 월드로부터 현재 드래그 중인 축 확인
	UWorld* World = GetWorld();
	EGizmoAxis ActiveAxis = (World != nullptr) ? World->GetDraggingAxis() : EGizmoAxis::None;

	// 3. 드래그 중이 아니라면, 마우스가 닿아있는(호버링) 축을 찾습니다.
	if (ActiveAxis == EGizmoAxis::None)
	{
		ActiveAxis = CheckGizmoPicking();
	}

	switch (ActiveAxis)
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
	ArrowZ->SetScale(ResultScale);
	BoundingSphere->SetScale(ResultScale);

	RootComponent->SetPosition(ArrowX->GetPosition() + ArrowX->GetForwardVector() * DeltaTime);
}

void UGizmoActor::Release()
{
	AActor::Release();
}
