#include "pch.h"
#include "ProjectileActor.h"

#include "Engine/Component/Primitive/StaticMeshComponent.h"
#include "Engine/Component/Particle/ParticleSystemComponent.h"
#include "Particle/ParticleSystem.h"
void AProjectileActor::BeginPlay()
{
	Super::BeginPlay();
}

void AProjectileActor::PostDuplicate()
{
	Super::PostDuplicate();

}


// 	SpringArm->AttachToComponent(CapsuleComponent);
// root 가 capsule component
void AProjectileActor::InitDefaultComponents()
{
	StaticMeshComponent = AddComponent<UStaticMeshComponent>();
	ParticleSystemComponent = AddComponent<UParticleSystemComponent>();

	SetRootComponent(StaticMeshComponent);
	ParticleSystemComponent->AttachToComponent(StaticMeshComponent);


	UParticleSystem* PS = UObjectManager::Get().CreateObject<UParticleSystem>();
	// BuildDemoTemplate(PS);

	ParticleSystemComponent->SetTemplate(PS);
	ParticleSystemComponent->Activate(false);

}