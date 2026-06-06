#include "BulletHellHealthProbeComponent.h"

#include <algorithm>

UBulletHellHealthProbeComponent::UBulletHellHealthProbeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bTickEnabled = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UBulletHellHealthProbeComponent::BeginPlay()
{
	UActorComponent::BeginPlay();

	if (bResetHealthOnBeginPlay)
	{
		ResetHealth();
	}
}

float UBulletHellHealthProbeComponent::ApplyDamage(float DamageAmount)
{
	const float ClampedDamage = (std::max)(0.0f, DamageAmount);
	if (ClampedDamage <= 0.0f || CurrentHealth <= 0.0f)
	{
		return 0.0f;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = (std::max)(0.0f, CurrentHealth - ClampedDamage);
	const float AppliedDamage = PreviousHealth - CurrentHealth;
	TotalDamageTaken += AppliedDamage;
	++HitCount;
	return AppliedDamage;
}

void UBulletHellHealthProbeComponent::ResetHealth()
{
	CurrentHealth = (std::max)(0.0f, MaxHealth);
	TotalDamageTaken = 0.0f;
	HitCount = 0;
}
