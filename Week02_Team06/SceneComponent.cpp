#include "pch.h"
#include "SceneComponent.h"
#include "Input.h"

void USceneComponent::TickComponent(float DeltaTime)
{
	if (UInput::GetInstance().IsKeyPressing('A'))
	{
		Position.X -= DeltaTime;
	}
	if (UInput::GetInstance().IsKeyPressing('D'))
	{
		Position.X += DeltaTime;
	}
	if (UInput::GetInstance().IsKeyPressing('S'))
	{
		Position.Y -= DeltaTime;
	}
	if (UInput::GetInstance().IsKeyPressing('W'))
	{
		Position.Y += DeltaTime;
	}
}
