#pragma once

#include "Component/ActorComponent.h"
#include "Core/Types/EngineTypes.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "Object/Ptr/WeakObjectPtr.h"

#include "Source/Engine/Component/Gameplay/BulletHellComponent.generated.h"

UENUM()
enum class EBulletHellDebugDrawMode : int32
{
	Off,
	All,
	Highlighted
};

UENUM()
enum class EBulletHellDebugSpawnPattern : int32
{
	Line,
	Ring,
	Radial
};

UENUM()
enum class EBulletHellRenderOrientationMode : int32
{
	Fixed,
	VelocityYaw
};

class UInstancedStaticMeshComponent;

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
	FVector Position = FVector::ZeroVector;
	FVector PreviousPosition = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float Radius = 1.0f;
	float Age = 0.0f;
	float Lifetime = 1.0f;
	int32 RenderInstanceIndex = -1;
	bool bAlive = true;
};

struct FBulletDebugStats
{
	int32 ActiveBulletCount = 0;
	uint32 TotalSpawned = 0;
	uint32 TotalKilled = 0;
	uint32 TotalExpired = 0;
	int32 DebugDrawSelectedCount = 0;
	int32 DebugDrawTruncatedCount = 0;
	int32 RenderInstanceCount = 0;
	int32 RendererSlotCount = 0;
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
	bool KillBullet(const FBulletHandle& Handle);
	bool IsBulletAlive(const FBulletHandle& Handle) const;
	const FBulletInstance* FindBullet(const FBulletHandle& Handle) const;

	UFUNCTION(Callable, Category="Bullet Hell")
	void ClearBullets();

	UFUNCTION(Pure, Category="Bullet Hell")
	int32 GetBulletCount() const;

	UFUNCTION(Callable, Category="Bullet Hell|Debug")
	int32 SpawnDebugPreset();

	UFUNCTION(Callable, Category="Bullet Hell|Debug")
	bool KillBulletById(int32 BulletId, int32 Generation);

	UFUNCTION(Callable, Category="Bullet Hell|Debug")
	bool KillRandomDebugBullet();

	UFUNCTION(Callable, Category="Bullet Hell|Debug")
	void LogBulletDebugStats() const;

	UFUNCTION(Callable, Category="Bullet Hell|Debug")
	void LogFirstBulletDebugInfo() const;

	FBulletDebugStats GetBulletDebugStats() const { return DebugStats; }
	FString GetBulletDebugStatsText() const;

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	void TickBullets(float DeltaTime);
	bool RemoveBulletAtIndex(int32 BulletIndex, bool bExpired);
	UInstancedStaticMeshComponent* EnsureRenderComponent();
	UInstancedStaticMeshComponent* GetRenderComponent() const;
	void ApplyRenderAssets();
	void RebuildRendererFromBullets();
	void SyncRenderInstancesBulk();
	void ClearRenderer();
	FTransform MakeBulletRenderTransform(const FBulletInstance& Bullet) const;
	void UpdateRenderDebugStats();
	void DrawBulletDebug();
	void DrawBulletCross(const FVector& Center, const FColor& Color, float Extent) const;
	FVector ResolveDebugSpawnOrigin() const;
	FVector ResolveDebugSpawnForward() const;
	FVector ResolveDebugSpawnRight() const;
	void ResetPerFrameDebugStats();
	void MaybeLogBulletDebugStats(float DeltaTime);

private:
	UPROPERTY(Edit, Save, Category="Bullet Hell|Debug", DisplayName="Auto Spawn Debug Preset")
	bool bAutoSpawnDebugPreset = false;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Debug", DisplayName="Draw Bullet Debug")
	bool bDrawBulletDebug = true;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Debug", DisplayName="Debug Draw Mode", Enum=EBulletHellDebugDrawMode)
	EBulletHellDebugDrawMode DebugDrawMode = EBulletHellDebugDrawMode::All;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Debug", DisplayName="Highlighted Bullet Id", Min=0, Max=1000000, Speed=1)
	int32 HighlightedBulletId = 0;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Debug", DisplayName="Debug Draw Max Count", Min=0, Max=1000000, Speed=1)
	int32 DebugDrawMaxCount = 512;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Debug", DisplayName="Debug Spawn Count", Min=0, Max=1000000, Speed=1)
	int32 DebugSpawnCount = 64;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Debug", DisplayName="Debug Spawn Speed", Min=0.0f, Max=100000.0f, Speed=1.0f)
	float DebugSpawnSpeed = 600.0f;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Debug", DisplayName="Debug Spawn Lifetime", Min=0.0f, Max=600.0f, Speed=0.1f)
	float DebugSpawnLifetime = 4.0f;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Debug", DisplayName="Debug Spawn Radius", Min=0.01f, Max=1000.0f, Speed=0.1f)
	float DebugSpawnRadius = 8.0f;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Debug", DisplayName="Debug Spawn Pattern", Enum=EBulletHellDebugSpawnPattern)
	EBulletHellDebugSpawnPattern DebugSpawnPattern = EBulletHellDebugSpawnPattern::Radial;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Render", DisplayName="Enable Rendering")
	bool bEnableRendering = true;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Render", DisplayName="Auto Create Renderer")
	bool bAutoCreateRenderer = true;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Render", DisplayName="Renderer Mesh Path", AssetType="StaticMesh")
	FString RendererMeshPath = "Content/Data/BasicShape/Sphere.OBJ";

	UPROPERTY(Edit, Save, Category="Bullet Hell|Render", DisplayName="Renderer Material Path", AssetType="Material")
	FString RendererMaterialPath = "None";

	UPROPERTY(Edit, Save, Category="Bullet Hell|Render", DisplayName="Render Scale", Min=0.01f, Max=1000.0f, Speed=0.1f)
	float RenderScale = 1.0f;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Render", DisplayName="Render Orientation Mode", Enum=EBulletHellRenderOrientationMode)
	EBulletHellRenderOrientationMode RenderOrientationMode = EBulletHellRenderOrientationMode::Fixed;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Debug", DisplayName="Log Debug Stats")
	bool bLogDebugStats = false;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Debug", DisplayName="Debug Stats Log Interval", Min=0.0f, Max=60.0f, Speed=0.1f)
	float DebugStatsLogInterval = 1.0f;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Debug Commands", DisplayName="Debug Spawn Request", Min=0, Max=1000000, Speed=1)
	int32 DebugSpawnRequest = 0;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Debug Commands", DisplayName="Debug Random Kill Request", Min=0, Max=1000000, Speed=1)
	int32 DebugRandomKillRequest = 0;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Debug Commands", DisplayName="Debug Clear Request", Min=0, Max=1000000, Speed=1)
	int32 DebugClearRequest = 0;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Debug Commands", DisplayName="Debug Log Stats Request", Min=0, Max=1000000, Speed=1)
	int32 DebugLogStatsRequest = 0;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Debug Commands", DisplayName="Debug Log First Bullet Request", Min=0, Max=1000000, Speed=1)
	int32 DebugLogFirstBulletRequest = 0;

	UPROPERTY(Edit, Save, Category="Bullet Hell|Debug Commands", DisplayName="Debug Rebuild Renderer Request", Min=0, Max=1000000, Speed=1)
	int32 DebugRebuildRendererRequest = 0;

	TArray<FBulletInstance> Bullets;
	TMap<uint32, int32> BulletIndexById;
	TWeakObjectPtr<UInstancedStaticMeshComponent> RenderComponent;
	uint32 NextBulletId = 1;
	uint32 NextBulletGeneration = 1;
	uint32 DebugKillRandomState = 0x9e3779b9u;
	float DebugStatsLogAccumulator = 0.0f;
	int32 LastDebugSpawnRequest = 0;
	int32 LastDebugRandomKillRequest = 0;
	int32 LastDebugClearRequest = 0;
	int32 LastDebugLogStatsRequest = 0;
	int32 LastDebugLogFirstBulletRequest = 0;
	int32 LastDebugRebuildRendererRequest = 0;
	FBulletDebugStats DebugStats;
};
