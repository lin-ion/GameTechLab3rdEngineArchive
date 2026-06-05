#include "LuaActorBindings.internal.h"

using namespace LuaActorBindingsDetail;

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
