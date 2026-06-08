#include "BulletHellDamageReceiverComponent.h"

#include "GameFramework/Camera/PlayerCameraManager.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "Runtime/Engine.h"

#include <algorithm>

UBulletHellDamageReceiverComponent::UBulletHellDamageReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bTickEnabled = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

float UBulletHellDamageReceiverComponent::ApplyDamage(float DamageAmount)
{
	if (!bDamageEnabled)
	{
		return 0.0f;
	}

	const float ClampedDamage = (std::max)(0.0f, DamageAmount);
	if (ClampedDamage <= 0.0f)
	{
		return 0.0f;
	}

	APawn* PawnOwner = GetOwnerPawn();
	if (!PawnOwner)
	{
		return 0.0f;
	}

	const float AppliedDamage = PawnOwner->GetDamaged(ClampedDamage);
	if (AppliedDamage > 0.0f)
	{
		TotalDamageForwarded += AppliedDamage;
		++HitCount;

		UWorld* World = GEngine ? GEngine->GetWorld() : nullptr;
		APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		if (PlayerController && PlayerController->GetPossessedPawn() == PawnOwner)
		{
			if (APlayerCameraManager* CameraManager = PlayerController->GetPlayerCameraManager())
			{
				CameraManager->StartDamageVignettePulse(0.5f);
			}
		}
	}
	return AppliedDamage;
}

APawn* UBulletHellDamageReceiverComponent::GetOwnerPawn() const
{
	return Cast<APawn>(GetOwner());
}
