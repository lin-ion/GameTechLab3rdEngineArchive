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
}

void UGizmoActor::Release()
{
	AActor::Release();
}
