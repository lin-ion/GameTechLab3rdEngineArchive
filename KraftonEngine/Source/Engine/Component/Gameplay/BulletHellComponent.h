#pragma once

#include "Component/ActorComponent.h"
#include "Core/Types/CollisionTypes.h"
#include "Core/Types/EngineTypes.h"
#include "Math/Quat.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "Object/Ptr/WeakObjectPtr.h"

#include "Source/Engine/Component/Gameplay/BulletHellComponent.generated.h"

UENUM()
enum class EBulletHellRenderOrientationMode : int32
{
	Fixed,
	VelocityYaw
};

class AActor;
class UInstancedStaticMeshComponent;

struct FBulletArchetype
{
	FString MeshPath = "Content/Data/BasicShape/Sphere.OBJ";
	FString MaterialPath = "None";
	float Radius = 1.0f;
	float Speed = 1.0f;
	float Lifetime = 1.0f;
	float RenderScale = 1.0f;
	float Damage = 1.0f;
};

struct FBulletRenderSlot
{
	FString MeshPath;
	FString MaterialPath;
	TWeakObjectPtr<UInstancedStaticMeshComponent> Renderer;
	TArray<int32> BulletIndices;
};

struct FBulletHandle
{
	uint32 Id = 0;
	uint32 Generation = 0;

	bool IsValid() const { return Id != 0 && Generation != 0; }
};

struct FBulletInstance
{
	uint32 Id = 0;
	uint32 Generation = 0;
	FString MeshPath = "Content/Data/BasicShape/Sphere.OBJ";
	FString MaterialPath = "None";
	FVector Position = FVector::ZeroVector;
	FVector PreviousPosition = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float Radius = 1.0f;
	float Damage = 1.0f;
	float Age = 0.0f;
	float Lifetime = 1.0f;
	int32 ArchetypeIndex = 0;	// Archetype = 비슷한 종류의 탄환들끼리의 묶음
	int32 GroupId = 0;			// Group = 동일 Archetype 내에서 특정 탄들만 골라서 파라미터를 변경하거나 하고 싶을 때 쓰는 '라벨'
	int32 RenderSlotIndex = -1;
	float RenderScale = 1.0f;
	FVector HomingTargetPosition = FVector::ZeroVector;
	TWeakObjectPtr<AActor> HomingTargetActor;
	bool bHoming = false;
	float HomingStrength = 0.0f;
	float HomingMaxTurnRateDegrees = 0.0f;
	float HomingConeHalfAngleDegrees = 180.0f;
	int32 RenderInstanceIndex = -1;
	bool bAlive = true;
};

struct FBulletSpawnParams
{
	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ForwardVector;
	FBulletArchetype Archetype;
	int32 ArchetypeIndex = 0;
	int32 GroupId = 0;
	FVector HomingTargetPosition = FVector::ZeroVector;
	TWeakObjectPtr<AActor> HomingTargetActor;
	bool bHoming = false;
	float HomingStrength = 0.0f;
	float HomingMaxTurnRateDegrees = 0.0f;
	float HomingConeHalfAngleDegrees = 180.0f;
};

struct FBulletRuntimeModifier
{
	int32 ArchetypeIndex = -1;
	int32 GroupId = -1;
	bool bOnlyHoming = false;
	bool bSetSpeed = false;
	float Speed = 0.0f;
	bool bSetHomingConeHalfAngle = false;
	float HomingConeHalfAngleDegrees = 180.0f;
	bool bSetHomingEnabled = false;
	bool bHoming = true;
};

struct FBulletLaunchParams
{
	FVector Velocity = FVector::ForwardVector;
	bool bSetHoming = true;
	bool bHoming = false;
	FVector HomingTargetPosition = FVector::ZeroVector;
	TWeakObjectPtr<AActor> HomingTargetActor;
	float HomingStrength = 0.0f;
	float HomingMaxTurnRateDegrees = 0.0f;
	float HomingConeHalfAngleDegrees = 180.0f;
	bool bResetAge = true;
	bool bSetLifetime = false;
	float Lifetime = 1.0f;
};

struct FBulletDebugStats
{
	int32 ActiveBulletCount = 0;
	uint32 TotalSpawned = 0;
	uint32 TotalKilled = 0;
	uint32 TotalExpired = 0;
	uint32 CollisionQueryCount = 0;
	uint32 CollisionHitCount = 0;
	uint32 CollisionKilledCount = 0;
	uint32 EraseKilledCount = 0;
	uint32 RuntimeModificationCount = 0;
	int32 ActiveNonHomingCount = 0;
	int32 ActiveHomingCount = 0;
	int32 ActivePrimaryArchetypeCount = 0;
	int32 ActiveSecondaryArchetypeCount = 0;
	int32 DebugDrawSelectedCount = 0;
	int32 DebugDrawTruncatedCount = 0;
	int32 RenderInstanceCount = 0;
	int32 RendererSlotCount = 0;
	int32 RendererSlot0InstanceCount = 0;
	int32 RendererSlot1InstanceCount = 0;
	int32 RenderMismatchCount = 0;
};

UCLASS()
class UBulletHellComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UBulletHellComponent();
	~UBulletHellComponent() override = default;

	void BeginPlay() override;
	void PostEditProperty(const char* PropertyName) override;

	FBulletHandle SpawnBullet(
		const FVector& Position,
		const FVector& Velocity,
		float Radius,
		float Lifetime);
	FBulletHandle SpawnBullet(const FBulletSpawnParams& Params);
	bool KillBullet(const FBulletHandle& Handle);
	bool KillBulletById(int32 BulletId, int32 Generation);
	bool IsBulletAlive(const FBulletHandle& Handle) const;
	const FBulletInstance* FindBullet(const FBulletHandle& Handle) const;
	const TArray<FBulletInstance>& GetBulletInstances() const { return Bullets; }
	bool LaunchBullet(const FBulletHandle& Handle, const FBulletLaunchParams& Params);
	int32 ApplyRuntimeModifier(const FBulletRuntimeModifier& Modifier);
	
	// =========== 대량 스폰용 헬퍼 함수들 ===========
	int32 SpawnSphereSurfaceToTarget(
		const FBulletSpawnParams& TemplateParams,
		const FVector& Center,
		float Radius,
		int32 Count,
		const FVector& Target,
		float Speed);
	int32 SpawnSphereSurfaceInDirection(
		const FBulletSpawnParams& TemplateParams,
		const FVector& Center,
		float Radius,
		int32 Count,
		const FVector& Direction,
		float Speed);
	int32 SpawnSphereVolumeRandomToTarget(
		const FBulletSpawnParams& TemplateParams,
		const FVector& Center,
		float Radius,
		int32 Count,
		const FVector& Target,
		float Speed);
	int32 SpawnSphereVolumeRandomInDirection(
		const FBulletSpawnParams& TemplateParams,
		const FVector& Center,
		float Radius,
		int32 Count,
		const FVector& Direction,
		float Speed);
	int32 SpawnCircleToTarget(
		const FBulletSpawnParams& TemplateParams,
		const FVector& Center,
		const FVector& Normal,
		float Radius,
		int32 Count,
		const FVector& Target,
		float Speed);
	int32 SpawnCircleInDirection(
		const FBulletSpawnParams& TemplateParams,
		const FVector& Center,
		const FVector& Normal,
		float Radius,
		int32 Count,
		const FVector& Direction,
		float Speed);
	int32 SpawnBoxToTarget(
		const FBulletSpawnParams& TemplateParams,
		const FVector& Center,
		int32 CountX,
		int32 CountY,
		int32 CountZ,
		float Spacing,
		const FQuat& Rotation,
		const FVector& Target,
		float Speed);
	int32 SpawnBoxInDirection(
		const FBulletSpawnParams& TemplateParams,
		const FVector& Center,
		int32 CountX,
		int32 CountY,
		int32 CountZ,
		float Spacing,
		const FQuat& Rotation,
		const FVector& Direction,
		float Speed);
	int32 SpawnLineToTarget(
		const FBulletSpawnParams& TemplateParams,
		const FVector& Start,
		const FVector& End,
		int32 Count,
		const FVector& Target,
		float Speed);
	int32 SpawnLineInDirection(
		const FBulletSpawnParams& TemplateParams,
		const FVector& Start,
		const FVector& End,
		int32 Count,
		const FVector& Direction,
		float Speed);

	UFUNCTION(Callable, Category="Bullet Hell|Homing")
	int32 SetActiveHomingConeHalfAngle(float ConeHalfAngleDegrees, int32 ArchetypeIndex);

	UFUNCTION(Callable, Category="Bullet Hell")
	void ClearBullets();

	UFUNCTION(Pure, Category="Bullet Hell")
	int32 GetBulletCount() const;

	FBulletDebugStats GetBulletDebugStats() const { return DebugStats; }
	void RecordDebugDrawStats(int32 SelectedCount, int32 TruncatedCount);

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	enum class EBulletCollisionKillReason
	{
		None,
		Collision,
		Erase
	};

	void TickBullets(float DeltaTime);
	void UpdateHomingBehavior(FBulletInstance& Bullet, float DeltaTime);
	void UpdateBehaviorDebugStats();
	EBulletCollisionKillReason CheckBulletCollision(const FBulletInstance& Bullet);
	bool SweepBulletByChannel(const FBulletInstance& Bullet, ECollisionChannel TraceChannel, FHitResult& OutHit);
	bool SweepBulletByObjectTypes(const FBulletInstance& Bullet, uint32 ObjectTypeMask, FHitResult& OutHit);
	uint32 BuildCollisionObjectTypeMask() const;
	uint32 BuildEraseObjectTypeMask() const;
	void ApplyDamageToHitTarget(const FBulletInstance& Bullet, const FHitResult& Hit) const;
	bool RemoveBulletAtIndex(int32 BulletIndex, bool bExpired);
	UInstancedStaticMeshComponent* EnsureRenderComponent();
	UInstancedStaticMeshComponent* GetRenderComponent() const;
	int32 FindOrCreateRenderSlot(const FBulletArchetype& Archetype);
	UInstancedStaticMeshComponent* EnsureRenderSlotComponent(int32 SlotIndex);
	UInstancedStaticMeshComponent* GetRenderSlotComponent(int32 SlotIndex) const;
	void ApplyRenderAssets();
	void ApplyRenderSlotAssets(int32 SlotIndex);
	void RebuildRendererFromBullets();
	void SyncRenderInstancesBulk();
	void ClearRenderer();
	FTransform MakeBulletRenderTransform(const FBulletInstance& Bullet) const;
	void UpdateRenderDebugStats();
	void ResetPerFrameDebugStats();
	void RecordOverlayStats() const;

private:
	UPROPERTY(Edit, Save, Category="Bullet Hell|Render", DisplayName="Enable Rendering")
	bool bEnableRendering = true;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Render", DisplayName="Auto Create Renderer")
	bool bAutoCreateRenderer = true;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Render", DisplayName="Renderer Mesh Path", AssetType="StaticMesh")
	FString RendererMeshPath = "Content/Data/BasicShape/Sphere.OBJ";

	UPROPERTY(Edit, Save, Category="Bullet Hell|Render", DisplayName="Renderer Material Path", AssetType="Material")
	FString RendererMaterialPath = "None";

	UPROPERTY(Edit, Save, Category="Bullet Hell|Render", DisplayName="Render Scale", Min=0.01f, Max=1000.0f, Speed=0.1f)
	float RenderScale = 0.1f;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Render", DisplayName="Render Orientation Mode", Enum=EBulletHellRenderOrientationMode)
	EBulletHellRenderOrientationMode RenderOrientationMode = EBulletHellRenderOrientationMode::Fixed;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Collision", DisplayName="Enable Collision")
	bool bEnableCollision = true;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Collision", DisplayName="Kill On Blocking Collision")
	bool bKillOnBlockingCollision = true;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Collision", DisplayName="Collision Trace Channel", Enum=ECollisionChannel)
	ECollisionChannel CollisionTraceChannel = ECollisionChannel::Projectile;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Collision", DisplayName="Kill On World Static")
	bool bKillOnWorldStatic = true;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Collision", DisplayName="Kill On World Dynamic")
	bool bKillOnWorldDynamic = false;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Collision", DisplayName="Kill On Pawn")
	bool bKillOnPawn = true;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Collision", DisplayName="Enable Erase Volumes")
	bool bEnableEraseVolumes = true;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Collision", DisplayName="Erase On Trigger")
	bool bEraseOnTrigger = true;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Collision", DisplayName="Erase On Projectile")
	bool bEraseOnProjectile = false;

	TArray<FBulletInstance> Bullets;
	TArray<FBulletRenderSlot> RenderSlots;
	TMap<uint32, int32> BulletIndexById;
	TWeakObjectPtr<UInstancedStaticMeshComponent> RenderComponent;
	uint32 NextBulletId = 1;
	uint32 NextBulletGeneration = 1;
	FBulletDebugStats DebugStats;
};
