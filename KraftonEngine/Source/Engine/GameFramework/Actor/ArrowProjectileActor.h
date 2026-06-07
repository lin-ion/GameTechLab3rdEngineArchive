#pragma once

#include "GameFramework/Actor/ProjectileActor.h"
#include "Object/Ptr/WeakObjectPtr.h"

class UStaticMeshComponent;

#include "Source/Engine/GameFramework/Actor/ArrowProjectileActor.generated.h"

UCLASS()
class AArrowProjectileActor : public AProjectileActor
{
public:
	GENERATED_BODY()
	AArrowProjectileActor() = default;

	void BeginPlay() override;
	void Tick(float DeltaTime) override;

	AActor* AsActor() override { return this; }
	void OnPoolConstruct() override;
	void Activate(const FVector& Location, const FVector& Velocity) override;
	void Deactivate() override;
	void ResetState() override;

	void HoldAt(const FVector& Location, const FVector& AimDirection);
	void Launch(const FVector& Velocity);

private:
	void InitArrowComponents();

private:
	TWeakObjectPtr<UStaticMeshComponent> StaticMeshComponent = nullptr;

	bool    bPoolConstructed = false;
	bool    bHeld = false;
	FVector CachedVelocity = FVector::ZeroVector;
	float   LifeTimeRemaining = 0.0f;
	float   VelocityWarmupRemaining = 0.0f;

	UPROPERTY(Edit, Save, Category = "Arrow Projectile", DisplayName = "Life Time")
	float DefaultLifeTime = 5.0f;
};
