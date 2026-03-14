#include "pch.h"
#include "PrimitiveComponent.h"

void UPrimitiveComponent::Release()
{
	USceneComponent::Release();
}

void UPrimitiveComponent::TickComponent(float DeltaTime)
{
	USceneComponent::TickComponent(DeltaTime);
}