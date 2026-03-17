#include "pch.h"
#include "GizmoActor.h"
#include "World.h"
#include "UArrowComponent.h"
#include "SphereComponent.h"

#include "FEditorViewportClient.h"

IMPLEMENT_CLASS(UGizmoActor, AActor)

void UGizmoActor::BeginPlay()
{
	ArrowY = AddComponent<UArrowComponent>();

	ArrowX = AddComponent<UArrowComponent>();
	ArrowX->SetRotation({ 0.0f, 0.0f, -90.0f });

	ArrowZ = AddComponent<UArrowComponent>();
	
	ArrowZ->SetRotation({ 90.0f, 0.0f, 0.0f });

	BasePoint = AddComponent<USphereComponent>();

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
}

void UGizmoActor::Release()
{
	AActor::Release();
}
