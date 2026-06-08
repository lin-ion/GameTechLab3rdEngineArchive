#pragma once

#include "Component/ActorComponent.h"
#include "Core/Types/CollisionTypes.h"
#include "Core/Types/EngineTypes.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "Object/Ptr/WeakObjectPtr.h"

#include "Source/Engine/Component/Gameplay/PlayerSprayProjectileComponent.generated.h"

class AActor;
class UBulletTrailComponent;
class UInstancedStaticMeshComponent;

struct FPlayerSprayProjectile
{
	FVector Position = FVector::ZeroVector;
	FVector PreviousPosition = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	TWeakObjectPtr<AActor> HomingTarget;
	float Age = 0.0f;
	float Lifetime = 2.0f;
	float ScatterAge = 0.0f;
	float Radius = 0.08f;
	float Damage = 1.0f;
	TArray<FVector> TrailSamples;
	float TrailSampleAccumulator = 0.0f;
	bool bHoming = false;
};

UCLASS()
class UPlayerSprayProjectileComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UPlayerSprayProjectileComponent();
	~UPlayerSprayProjectileComponent() override = default;

	void BeginPlay() override;

	UFUNCTION(Callable, Category="Player Spray")
	void StartAttack();

	UFUNCTION(Callable, Category="Player Spray")
	void StopAttack();

	UFUNCTION(Pure, Category="Player Spray")
	bool IsAttacking() const { return bAttackHeld; }

	UFUNCTION(Callable, Category="Player Spray")
	void ClearProjectiles();

	UFUNCTION(Pure, Category="Player Spray")
	int32 GetProjectileCount() const { return static_cast<int32>(Projectiles.size()); }

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	void TickAttack(float DeltaTime);
	void TryFireBurst();
	bool FindCameraBossTarget(FVector& OutCameraLocation, FVector& OutCameraForward, AActor*& OutBossActor);
	bool IsBossActor(const AActor* Candidate) const;
	bool RaycastBossVisualFallback(
		UWorld* World,
		const FVector& Start,
		const FVector& Direction,
		float MaxDistance,
		FHitResult& OutHit,
		AActor*& OutBossActor) const;
	bool RayIntersectsBox(
		const FVector& Start,
		const FVector& Direction,
		float MaxDistance,
		const FBoundingBox& Bounds,
		float& OutDistance) const;
	void SpawnProjectile(const FVector& Origin, const FVector& CameraForward, AActor* TargetActor, int32 BurstIndex, int32 BurstCount);
	void TickProjectiles(float DeltaTime);
	void UpdateHoming(FPlayerSprayProjectile& Projectile, float DeltaTime);
	bool CheckProjectileCollision(const FPlayerSprayProjectile& Projectile);
	bool CheckBossPhysicsAssetHit(const FPlayerSprayProjectile& Projectile, AActor* BossActor, FHitResult& OutHit) const;
	void ApplyDamageToHitTarget(const FPlayerSprayProjectile& Projectile, const FHitResult& Hit) const;
	void RemoveProjectileAtIndex(int32 ProjectileIndex);
	void SyncRender();
	void SyncTrail();
	void ClearRender();
	UInstancedStaticMeshComponent* EnsureRenderComponent();
	UBulletTrailComponent* EnsureTrailComponent();
	FTransform MakeProjectileTransform(const FPlayerSprayProjectile& Projectile) const;
	FVector BuildSpreadDirection(const FVector& CameraForward, int32 BurstIndex, int32 BurstCount) const;

private:
	UPROPERTY(Edit, Save, Category="Player Spray|Fire", DisplayName="Fire Rate", Min=0.1f, Max=120.0f, Speed=0.1f)
	float FireRate = 12.0f;

	UPROPERTY(Edit, Save, Category="Player Spray|Fire", DisplayName="Projectiles Per Burst", Min=1, Max=64, Speed=1)
	int32 ProjectilesPerBurst = 8;

	UPROPERTY(Edit, Save, Category="Player Spray|Fire", DisplayName="Aim Ray Distance", Min=1.0f, Max=100000.0f, Speed=1.0f)
	float AimRayDistance = 1000.0f;

	UPROPERTY(Edit, Save, Category="Player Spray|Fire", DisplayName="Spawn Forward Offset", Min=-100.0f, Max=100.0f, Speed=0.1f)
	float SpawnForwardOffset = 0.7f;

	UPROPERTY(Edit, Save, Category="Player Spray|Fire", DisplayName="Spawn Up Offset", Min=-100.0f, Max=100.0f, Speed=0.1f)
	float SpawnUpOffset = 1.0f;

	UPROPERTY(Edit, Save, Category="Player Spray|Projectile", DisplayName="Initial Speed", Min=0.0f, Max=1000.0f, Speed=0.1f)
	float InitialSpeed = 7.0f;

	UPROPERTY(Edit, Save, Category="Player Spray|Projectile", DisplayName="Homing Speed", Min=0.0f, Max=1000.0f, Speed=0.1f)
	float HomingSpeed = 11.0f;

	UPROPERTY(Edit, Save, Category="Player Spray|Projectile", DisplayName="Scatter Duration", Min=0.0f, Max=10.0f, Speed=0.01f)
	float ScatterDuration = 0.25f;

	UPROPERTY(Edit, Save, Category="Player Spray|Projectile", DisplayName="Homing Strength", Min=0.0f, Max=100.0f, Speed=0.1f)
	float HomingStrength = 8.0f;

	UPROPERTY(Edit, Save, Category="Player Spray|Projectile", DisplayName="Homing Max Turn Rate", Min=0.0f, Max=3600.0f, Speed=1.0f)
	float HomingMaxTurnRateDegrees = 720.0f;

	UPROPERTY(Edit, Save, Category="Player Spray|Projectile", DisplayName="Cone Half Angle", Min=0.0f, Max=89.0f, Speed=0.1f)
	float ConeHalfAngleDegrees = 22.0f;

	UPROPERTY(Edit, Save, Category="Player Spray|Projectile", DisplayName="Projectile Radius", Min=0.001f, Max=100.0f, Speed=0.01f)
	float ProjectileRadius = 0.08f;

	UPROPERTY(Edit, Save, Category="Player Spray|Projectile", DisplayName="Projectile Lifetime", Min=0.01f, Max=60.0f, Speed=0.1f)
	float ProjectileLifetime = 2.5f;

	UPROPERTY(Edit, Save, Category="Player Spray|Projectile", DisplayName="Projectile Damage", Min=0.0f, Max=1000000.0f, Speed=1.0f)
	float ProjectileDamage = 1.0f;

	UPROPERTY(Edit, Save, Category="Player Spray|Render", DisplayName="Mesh Path", AssetType="StaticMesh")
	FString MeshPath = "Content/Data/BasicShape/Sphere.OBJ";

	UPROPERTY(Edit, Save, Category="Player Spray|Render", DisplayName="Material Path", AssetType="Material")
	FString MaterialPath = "Content/Material/Particle/ParticleBeamTrail.uasset";

	UPROPERTY(Edit, Save, Category="Player Spray|Render", DisplayName="Render Scale", Min=0.001f, Max=100.0f, Speed=0.01f)
	float RenderScale = 0.08f;

	UPROPERTY(Edit, Save, Category="Player Spray|Trail", DisplayName="Trail Material Path", AssetType="Material")
	FString TrailMaterialPath = "Content/Material/Particle/ParticleBeamTrail.uasset";

	UPROPERTY(Edit, Save, Category="Player Spray|Trail", DisplayName="Trail Color", Type=Color4)
	FVector4 TrailColor = FVector4(0.45f, 0.75f, 1.0f, 1.0f);

	UPROPERTY(Edit, Save, Category="Player Spray|Trail", DisplayName="Trail Width", Min=0.001f, Max=10.0f, Speed=0.01f)
	float TrailWidth = 0.06f;

	UPROPERTY(Edit, Save, Category="Player Spray|Trail", DisplayName="Trail Lifetime", Min=0.01f, Max=10.0f, Speed=0.01f)
	float TrailLifetime = 0.18f;

	UPROPERTY(Edit, Save, Category="Player Spray|Trail", DisplayName="Trail Max Samples", Min=2, Max=128, Speed=1)
	int32 TrailMaxSamples = 8;

	UPROPERTY(Edit, Save, Category="Player Spray|Debug", DisplayName="Draw Aim Ray")
	bool bDrawAimRay = true;

	UPROPERTY(Edit, Save, Category="Player Spray|Debug", DisplayName="Aim Ray Debug Duration", Min=0.0f, Max=10.0f, Speed=0.01f)
	float AimRayDebugDuration = 0.05f;

	TArray<FPlayerSprayProjectile> Projectiles;
	TWeakObjectPtr<UInstancedStaticMeshComponent> RenderComponent;
	TWeakObjectPtr<UBulletTrailComponent> TrailComponent;
	float FireAccumulator = 0.0f;
	bool bAttackHeld = false;
};
