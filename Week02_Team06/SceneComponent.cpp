#include "pch.h"
#include "SceneComponent.h"
#include "Input.h"

void USceneComponent::Release()
{
}

void USceneComponent::Update(float DeltaTime)
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

void USceneComponent::Render(ID3D11DeviceContext& DeviceContext)
{
}
