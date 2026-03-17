#include "pch.h"
#include "GizmoActor.h"
#include "World.h"
#include "UArrowComponent.h"
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
}

void UGizmoActor::Release()
{
	AActor::Release();
}
