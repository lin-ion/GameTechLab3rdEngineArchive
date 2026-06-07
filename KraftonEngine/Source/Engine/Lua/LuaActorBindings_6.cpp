#include "LuaActorBindings.internal.h"
#include "GameFramework/ProjectilePoolSubSystem.h"   // FProjectilePoolSubsystem::Acquire<T>
#include "GameFramework/Actor/ProjectileActor.h"     // AProjectileActor
#include "GameFramework/Actor/ArrowProjectileActor.h"
#include "Component/Gameplay/PlayerSprayProjectileComponent.h"

using namespace LuaActorBindingsDetail;

namespace
{
    bool GetCameraProjectileFrame(UWorld* W, FVector& OutOrigin, FRotator& OutRotation)
    {
        if (!W) return false;

        FMinimalViewInfo POV;
        if (!W->GetActivePOV(POV)) return false;

        OutOrigin = POV.Location;
        OutRotation = POV.Rotation;
        if (APlayerController* PC = W->GetFirstPlayerController())
        {
            if (APawn* Pawn = PC->GetPossessedPawn())
            {
                OutOrigin = Pawn->GetActorLocation();
            }
        }
        return true;
    }

    FVector ComputeCameraProjectileLocation(const FVector& Origin, const FRotator& Rotation, const FVector& Offset)
    {
        return Origin
            + Rotation.GetForwardVector() * Offset.X
            + Rotation.GetRightVector() * Offset.Y
            + Rotation.GetUpVector() * Offset.Z;
    }
}

void FLuaScriptManager::RegisterActorBindings_6(sol::state& Lua)
{
    sol::table World = Lua.create_named_table("World");
    World.set_function(
        "GetFirstPlayerController",
        []() -> APlayerController*
        {
            return (GEngine && GEngine->GetWorld()) ? GEngine->GetWorld()->GetFirstPlayerController() : nullptr;
        }
    );
    World.set_function(
        "SpawnActor",
        [](const FString& ClassName, sol::optional<FVector> Location, sol::optional<FVector> Rotation, sol::optional<FVector> Scale) -> AActor*
        {
            if (!GEngine) return nullptr;
            UWorld* W = GEngine->GetWorld();
            if (!W) return nullptr;
            UClass* Cls = UClass::FindByName(ClassName.c_str());
            if (!Cls) return nullptr;
            AActor* Actor = W->SpawnActorByClass(Cls);
            if (IsValid(Actor))
            {
                Actor->SetActorLocation(Location.value_or(FVector(0, 0, 0)));
                Actor->SetActorRotation(Rotation.value_or(FVector(0, 0, 0)));
                Actor->SetActorScale(Scale.value_or(FVector(1, 1, 1)));
            }
            return Actor;
        }
    );
    World.set_function(
        "SpawnEmitterAtLocation",
        [](const FString& TemplatePath, const FVector& Location, sol::optional<FVector> Rotation, sol::optional<bool> bActivate) -> UParticleSystemComponent*
        {
            if (!GEngine) return nullptr;
            UWorld* W = GEngine->GetWorld();
            if (!W) return nullptr;
            return FGameplayStatics::SpawnEmitterAtLocation(
                W,
                TemplatePath,
                Location,
                FRotator(Rotation.value_or(FVector(0, 0, 0))),
                bActivate.value_or(true)
            );
        }
    );
    World.set_function(
        "FireCameraProjectile",
        [](sol::optional<float> Speed, sol::optional<float> MuzzleOffset, sol::optional<float> SpawnHeight) -> AActor*
        {
            if (!GEngine) return nullptr;
            UWorld* W = GEngine->GetWorld();
            if (!W) return nullptr;

            FProjectilePoolSubsystem* Pool = W->GetProjectilePool();
            if (!Pool) return nullptr;

            // 조준 방향 = 카메라 시점(3인칭에서도 바라보는 방향).
            FMinimalViewInfo POV;
            if (!W->GetActivePOV(POV)) return nullptr;
            const FVector Forward = POV.Rotation.GetForwardVector()*2;

            // 발사 원점 = (3인칭) 카메라가 아니라 '플레이어가 조종하는 폰(Haru)' 위치.
            //   PlayerController::GetPossessedPawn() → APawn(=AActor) → GetActorLocation().
            //   폰을 못 찾으면 카메라 위치로 폴백(기존 동작).
            FVector Origin = POV.Location;
            if (APlayerController* PC = W->GetFirstPlayerController())
            {
                if (APawn* Pawn = PC->GetPossessedPawn())
                {
                    Origin = Pawn->GetActorLocation();
                }
            }

            const float   SpeedVal  = Speed.value_or(0.5f);        // m/s
            const float   OffsetVal = MuzzleOffset.value_or(1.0f);  // m, 조준 방향 전방 오프셋
            const float   HeightVal = SpawnHeight.value_or(1.0f);   // m, 캐릭터 기준 높이(가슴 정도)
            const FVector SpawnLoc  = Origin + Forward * OffsetVal + FVector(0.0f, 0.0f, HeightVal);

            // ── [진단] 발사 직전 월드 상태 + Acquire 결과 ──
            const FVector Vel = Forward * SpeedVal;
            AActor* Proj = Pool->Acquire<AProjectileActor>(SpawnLoc, Vel);
            UE_LOG("[ProjFire] PIE=%d type=%d spawn=(%.2f,%.2f,%.2f) vel=(%.3f,%.3f,%.3f) acquired=%d",
                (int)W->HasBegunPlay(), (int)W->GetWorldType(),
                SpawnLoc.X, SpawnLoc.Y, SpawnLoc.Z, Vel.X, Vel.Y, Vel.Z, (int)(Proj != nullptr));
            return Proj;
        }
    );
    World.set_function(
        "FireCameraArrowProjectile",
        [](sol::optional<float> Speed, sol::optional<FVector> SpawnOffset) -> AActor*
        {
            if (!GEngine) return nullptr;
            UWorld* W = GEngine->GetWorld();
            if (!W) return nullptr;

            FProjectilePoolSubsystem* Pool = W->GetProjectilePool();
            if (!Pool) return nullptr;

            FVector Origin;
            FRotator Rotation;
            if (!GetCameraProjectileFrame(W, Origin, Rotation)) return nullptr;

            const float   SpeedVal  = Speed.value_or(8.0f);
            const FVector OffsetVal = SpawnOffset.value_or(FVector(1.0f, 0.0f, 1.0f));
            const FVector Forward   = Rotation.GetForwardVector();
            const FVector SpawnLoc  = ComputeCameraProjectileLocation(Origin, Rotation, OffsetVal);
            const FVector Vel       = Forward * SpeedVal;

            AActor* Proj = Pool->Acquire<AArrowProjectileActor>(SpawnLoc, Vel);
            UE_LOG("[ArrowProjectileFire] PIE=%d type=%d spawn=(%.2f,%.2f,%.2f) vel=(%.3f,%.3f,%.3f) acquired=%d",
                (int)W->HasBegunPlay(), (int)W->GetWorldType(),
                SpawnLoc.X, SpawnLoc.Y, SpawnLoc.Z, Vel.X, Vel.Y, Vel.Z, (int)(Proj != nullptr));
            return Proj;
        }
    );
    World.set_function(
        "PrepareCameraArrowProjectile",
        [](sol::optional<FVector> SpawnOffset) -> AActor*
        {
            if (!GEngine) return nullptr;
            UWorld* W = GEngine->GetWorld();
            if (!W) return nullptr;

            FProjectilePoolSubsystem* Pool = W->GetProjectilePool();
            if (!Pool) return nullptr;

            FVector Origin;
            FRotator Rotation;
            if (!GetCameraProjectileFrame(W, Origin, Rotation)) return nullptr;

            const FVector OffsetVal = SpawnOffset.value_or(FVector(1.0f, 0.0f, 1.0f));
            const FVector SpawnLoc = ComputeCameraProjectileLocation(Origin, Rotation, OffsetVal);
            AArrowProjectileActor* Proj = Pool->Acquire<AArrowProjectileActor>(SpawnLoc, FVector::ZeroVector);
            if (Proj)
            {
                Proj->HoldAt(SpawnLoc, Rotation.GetForwardVector());
            }
            UE_LOG("[ArrowProjectilePrepare] spawn=(%.2f,%.2f,%.2f) acquired=%d",
                SpawnLoc.X, SpawnLoc.Y, SpawnLoc.Z, (int)(Proj != nullptr));
            return Proj;
        }
    );
    World.set_function(
        "UpdateCameraArrowProjectile",
        [](AActor* Projectile, sol::optional<FVector> SpawnOffset) -> bool
        {
            AArrowProjectileActor* Arrow = Cast<AArrowProjectileActor>(Projectile);
            if (!Arrow || !GEngine) return false;
            UWorld* W = GEngine->GetWorld();
            if (!W) return false;

            FVector Origin;
            FRotator Rotation;
            if (!GetCameraProjectileFrame(W, Origin, Rotation)) return false;

            const FVector OffsetVal = SpawnOffset.value_or(FVector(1.0f, 0.0f, 1.0f));
            Arrow->HoldAt(ComputeCameraProjectileLocation(Origin, Rotation, OffsetVal), Rotation.GetForwardVector());
            return true;
        }
    );
    World.set_function(
        "LaunchCameraArrowProjectile",
        [](AActor* Projectile, sol::optional<float> Speed, sol::optional<FVector> SpawnOffset) -> bool
        {
            AArrowProjectileActor* Arrow = Cast<AArrowProjectileActor>(Projectile);
            if (!Arrow || !GEngine) return false;
            UWorld* W = GEngine->GetWorld();
            if (!W) return false;

            FVector Origin;
            FRotator Rotation;
            if (!GetCameraProjectileFrame(W, Origin, Rotation)) return false;

            const float SpeedVal = Speed.value_or(8.0f);
            const FVector OffsetVal = SpawnOffset.value_or(FVector(1.0f, 0.0f, 1.0f));
            const FVector Forward = Rotation.GetForwardVector();
            Arrow->SetActorLocation(ComputeCameraProjectileLocation(Origin, Rotation, OffsetVal));
            Arrow->Launch(Forward * SpeedVal);
            return true;
        }
    );
    World.set_function(
        "ReleaseProjectile",
        [](AActor* Projectile) -> bool
        {
            if (!Projectile || !GEngine) return false;
            UWorld* W = GEngine->GetWorld();
            if (!W) return false;
            FProjectilePoolSubsystem* Pool = W->GetProjectilePool();
            if (!Pool) return false;
            Pool->Release(Projectile);
            return true;
        }
    );
    World.set_function(
        "StartPlayerSprayAttack",
        [](AActor* Owner) -> bool
        {
            if (!Owner) return false;

            UPlayerSprayProjectileComponent* Spray = Owner->GetComponentByClass<UPlayerSprayProjectileComponent>();
            const bool bHadSprayComponent = Spray != nullptr;
            if (!Spray)
            {
                Spray = Owner->AddComponent<UPlayerSprayProjectileComponent>();
                if (Spray)
                {
                    Spray->SetFName(FName("PlayerSprayProjectileComponent"));
                    if (Owner->HasActorBegunPlay())
                    {
                        Spray->BeginPlay();
                    }
                }
            }

            if (!Spray) return false;
            UE_LOG("[PlayerSpray] Lua StartPlayerSprayAttack owner=%s component=%s source=%s",
                Owner->GetName().c_str(),
                Spray->GetName().c_str(),
                bHadSprayComponent ? "existing" : "auto-created");
            Spray->StartAttack();
            return true;
        }
    );
    World.set_function(
        "StopPlayerSprayAttack",
        [](AActor* Owner) -> bool
        {
            if (!Owner) return false;

            UPlayerSprayProjectileComponent* Spray = Owner->GetComponentByClass<UPlayerSprayProjectileComponent>();
            if (!Spray) return false;
            Spray->StopAttack();
            return true;
        }
    );
    World.set_function(
        "SpawnPawn",
        [](const FString& ClassName, sol::optional<FVector> Location, sol::optional<FVector> Rotation, sol::optional<FVector> Scale, sol::optional<bool> bPossess) -> APawn*
        {
            if (!GEngine) return nullptr;
            UWorld* W = GEngine->GetWorld();
            if (!W) return nullptr;
            UClass* Cls = UClass::FindByName(ClassName.c_str());
            if (!Cls) return nullptr;
            AActor* Actor = W->SpawnActorByClass(Cls);
            APawn*  Pawn  = Cast<APawn>(Actor);
            if (!IsValid(Pawn))
            {
                if (IsValid(Actor)) W->DestroyActor(Actor);
                return nullptr;
            }
            Pawn->SetActorLocation(Location.value_or(FVector(0, 0, 0)));
            Pawn->SetActorRotation(Rotation.value_or(FVector(0, 0, 0)));
            Pawn->SetActorScale(Scale.value_or(FVector(1, 1, 1)));
            if (bPossess.value_or(false))
            {
                if (APlayerController* PC = W->GetFirstPlayerController()) PC->Possess(Pawn);
            }
            return Pawn;
        }
    );
    World.set_function(
        "FindActorByName",
        [](const FString& ActorName) -> AActor*
        {
            if (!GEngine || !GEngine->GetWorld()) return nullptr;
            UWorld* W = GEngine->GetWorld();
            for (AActor* Actor : W->GetActors())
            {
                if (IsValid(Actor) && Actor->GetFName().ToString() == ActorName)
                {
                    return Actor;
                }
            }
            return nullptr;
        }
    );
    World.set_function(
        "FindFirstActorByClass",
        [](const FString& ClassName) -> AActor*
        {
            if (!GEngine || !GEngine->GetWorld()) return nullptr;
            UWorld* W   = GEngine->GetWorld();
            UClass* Cls = UClass::FindByName(ClassName.c_str());
            if (!Cls) return nullptr;
            for (AActor* Actor : W->GetActors())
            {
                if (IsValid(Actor) && Actor->GetClass()->IsA(Cls))
                {
                    return Actor;
                }
            }
            return nullptr;
        }
    );
    World.set_function(
        "FindFirstActorByTag",
        [](const FString& Tag) -> AActor*
        {
            return FGameplayStatics::FindFirstActorByTag(
                GEngine ? GEngine->GetWorld() : nullptr,
                FName(Tag)
            );
        }
    );
    World.set_function(
        "FindActorsByTag",
        [](const FString& Tag) -> sol::table
        {
            sol::table            Result = FLuaScriptManager::GetState().create_table();
            const TArray<AActor*> Found  = FGameplayStatics::FindActorsByTag(
                GEngine ? GEngine->GetWorld() : nullptr,
                FName(Tag)
            );
            int Idx = 1; // Lua arrays are 1-indexed
            for (AActor* Actor : Found)
            {
                Result[Idx++] = Actor;
            }
            return Result;
        }
    );
    // LuaBlueprint ForEachActorByClass 노드용 — 동일 패턴(table 반환)으로 노출.
    World.set_function(
        "FindActorsByClass",
        [](const FString& ClassName) -> sol::table
        {
            sol::table Result = FLuaScriptManager::GetState().create_table();
            if (!GEngine || !GEngine->GetWorld()) return Result;
            UClass* Cls = UClass::FindByName(ClassName.c_str());
            if (!Cls)
            {
                static TSet<FString> WarnedUnknownClasses;
                if (WarnedUnknownClasses.find(ClassName) == WarnedUnknownClasses.end())
                {
                    WarnedUnknownClasses.insert(ClassName);
                    UE_LOG(
                        "World.FindActorsByClass: 등록되지 않은 액터 클래스 '%s' — 빈 리스트 반환 "
                        "(클래스 이름 오타/미설정 확인)",
                        ClassName.c_str()
                    );
                }
                return Result;
            }
            int Idx = 1;
            for (AActor* Actor : GEngine->GetWorld()->GetActors())
            {
                if (IsValid(Actor) && Actor->GetClass() && Actor->GetClass()->IsA(Cls))
                {
                    Result[Idx++] = Actor;
                }
            }
            return Result;
        }
    );
    World.set_function(
        "GetGameTime",
        []() -> float
        {
            UWorld* CurrentWorld = GEngine ? GEngine->GetWorld() : nullptr;
            return CurrentWorld ? CurrentWorld->GetGameTimeSeconds() : 0.0f;
        }
    );
    World.set_function(
        "LineTrace",
        [](const FVector& Start, const FVector& End, sol::optional<AActor*> IgnoreActor) -> sol::table
        {
            sol::table Result   = FLuaScriptManager::GetState().create_table();
            Result["Hit"]       = false;
            Result["Actor"]     = static_cast<AActor*>(nullptr);
            Result["Component"] = static_cast<UPrimitiveComponent*>(nullptr);
            Result["Location"]  = FVector(0, 0, 0);
            Result["Normal"]    = FVector(0, 0, 0);
            Result["Distance"]  = 0.0f;

            UWorld* CurrentWorld = GEngine ? GEngine->GetWorld() : nullptr;
            if (!CurrentWorld)
            {
                return Result;
            }

            FVector     Delta       = End - Start;
            const float MaxDistance = Delta.Length();
            if (MaxDistance <= 0.0001f)
            {
                return Result;
            }
            const FVector Direction = Delta / MaxDistance;
            FHitResult    Hit;
            if (CurrentWorld->PhysicsRaycast(Start, Direction, MaxDistance, Hit, ECollisionChannel::WorldStatic, IgnoreActor.value_or(nullptr)))
            {
                Result["Hit"]       = true;
                Result["Actor"]     = Hit.HitActor;
                Result["Component"] = Hit.HitComponent;
                Result["Location"]  = Hit.WorldHitLocation;
                Result["Normal"]    = Hit.WorldNormal;
                Result["Distance"]  = Hit.Distance;
            }
            return Result;
        }
    );
}
