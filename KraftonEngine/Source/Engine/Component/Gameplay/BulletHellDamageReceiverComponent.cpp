#include "BulletHellDamageReceiverComponent.h"

#include "GameFramework/Pawn/Pawn.h"

#include <algorithm>

UBulletHellDamageReceiverComponent::UBulletHellDamageReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bTickEnabled = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

float UBulletHellDamageReceiverComponent::ApplyDamage(float DamageAmount)
{
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
	}
	return AppliedDamage;
}

APawn* UBulletHellDamageReceiverComponent::GetOwnerPawn() const
{
	return Cast<APawn>(GetOwner());
}
