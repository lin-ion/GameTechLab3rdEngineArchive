#pragma once
#include "GameFramework/AActor.h"
#include "Object/Ptr/WeakObjectPtr.h"

class UParticleSystemComponent;
class UStaticMeshComponent;
#include "Source/Engine/GameFramework/Actor/ProjectileActor.generated.h"


UCLASS()
class AProjectileActor : public AActor
{
public:
	GENERATED_BODY()
	AProjectileActor() = default;
	
	void InitDefaultComponents();
	void PostDuplicate() override;
	void BeginPlay() override;

	UFUNCTION(Pure, Category = "Actor|Components")
	UStaticMeshComponent* GetStaticComponent() const { return StaticMeshComponent; }
	
	UFUNCTION(Pure, Category = "Actor|Components")
	UParticleSystemComponent* GetParticleSystemComponent() const { return ParticleSystemComponent; }


private:
	TWeakObjectPtr<UStaticMeshComponent> StaticMeshComponent = nullptr;
	TWeakObjectPtr<UParticleSystemComponent> ParticleSystemComponent = nullptr;

};