#pragma once

#include "Component/Gameplay/BossPatternComponentBase.h"
#include "Component/Gameplay/BulletHellComponent.h"

#include "Source/Engine/Component/Gameplay/BossPattern_SphericalPulseBarrage.generated.h"

UCLASS()
class UBossPattern_SphericalPulseBarrage : public UBossPatternComponentBase
{
public:
	GENERATED_BODY()
	UBossPattern_SphericalPulseBarrage();
	~UBossPattern_SphericalPulseBarrage() override = default;

	bool GetCanUse(const FBossPatternContext& Context, FString* OutRejectReason) const override;
	FString GetRuntimeDebugText() const override;

protected:
	void OnPatternStart(const FBossPatternContext& Context) override;
	void TickCurrentStep(float DeltaTime, const FBossPatternContext& Context) override;
	bool ShouldAdvanceStep(const FBossPatternContext& Context) const override;
	EBossPatternStep GetNextStep(EBossPatternStep CurrentStep) const override;

private:
	void TickPulseSpawning(const FBossPatternContext& Context);
	void SpawnPulse(const FBossPatternContext& Context);
	FBulletSpawnParams MakeBulletParams(const FVector& Position, const FVector& Direction) const;
	float GetPulseInterval() const;
	FVector MakeSphereDirection(int32 Index, int32 Count, int32 PulseIndex) const;

private:
	UPROPERTY(Edit, Save, Category="Boss Pattern|Spherical Pulse Barrage", DisplayName="Pulse Duration", Min=0.0f, Max=60.0f, Speed=0.01f)
	float PulseDuration = 2.0f;

	UPROPERTY(Edit, Save, Category="Boss Pattern|Spherical Pulse Barrage", DisplayName="Pulse Count", Min=1, Max=128, Speed=1)
	int32 PulseCount = 4;

	UPROPERTY(Edit, Save, Category="Boss Pattern|Spherical Pulse Barrage", DisplayName="Pulse Interval Override", Min=0.0f, Max=30.0f, Speed=0.01f)
	float PulseIntervalOverride = 0.0f;

	UPROPERTY(Edit, Save, Category="Boss Pattern|Spherical Pulse Barrage", DisplayName="Projectiles Per Pulse", Min=1, Max=2048, Speed=1)
	int32 ProjectilesPerPulse = 48;

	UPROPERTY(Edit, Save, Category="Boss Pattern|Spherical Pulse Barrage", DisplayName="Sphere Radius", Min=0.0f, Max=1000.0f, Speed=0.1f)
	float SphereRadius = 2.0f;

	UPROPERTY(Edit, Save, Category="Boss Pattern|Spherical Pulse Barrage", DisplayName="Projectile Speed", Min=0.0f, Max=1000.0f, Speed=0.1f)
	float ProjectileSpeed = 8.0f;

	UPROPERTY(Edit, Save, Category="Boss Pattern|Spherical Pulse Barrage", DisplayName="Projectile Lifetime", Min=-1.0f, Max=300.0f, Speed=0.1f)
	float ProjectileLifetime = 5.0f;

	UPROPERTY(Edit, Save, Category="Boss Pattern|Spherical Pulse Barrage", DisplayName="Projectile Radius", Min=0.01f, Max=100.0f, Speed=0.01f)
	float ProjectileRadius = 0.22f;

	UPROPERTY(Edit, Save, Category="Boss Pattern|Spherical Pulse Barrage", DisplayName="Projectile Render Scale", Min=0.01f, Max=100.0f, Speed=0.01f)
	float ProjectileRenderScale = 0.18f;

	UPROPERTY(Edit, Save, Category="Boss Pattern|Spherical Pulse Barrage", DisplayName="Projectile Mesh Path", AssetType="StaticMesh")
	FString ProjectileMeshPath = "Content/Data/BasicShape/Sphere.OBJ";

	UPROPERTY(Edit, Save, Category="Boss Pattern|Spherical Pulse Barrage", DisplayName="Projectile Material Path", AssetType="Material")
	FString ProjectileMaterialPath = "None";

	UPROPERTY(Edit, Save, Category="Boss Pattern|Spherical Pulse Barrage", DisplayName="Use Random Sphere Points")
	bool bUseRandomSpherePoints = false;

	UPROPERTY(Edit, Save, Category="Boss Pattern|Spherical Pulse Barrage", DisplayName="Random Seed Offset")
	int32 RandomSeedOffset = 0;

	int32 SpawnedPulseCount = 0;
	float NextPulseTime = 0.0f;
};
