#include "GameFramework/PrimitiveActors.h"

#include "Component/BillboardComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/DecalComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/FireBallComponent.h"
#include "Component/HeightFogComponent.h"
#include "Core/ResourceManager.h"
#include <format>

#ifndef NO_UUID
#define NO_UUID 1
#endif

DEFINE_CLASS(AStaticMeshActor, AActor)
REGISTER_FACTORY(AStaticMeshActor)

DEFINE_CLASS(ASubUVActor, AActor)
REGISTER_FACTORY(ASubUVActor)

DEFINE_CLASS(ATextRenderActor, AActor)
REGISTER_FACTORY(ATextRenderActor)

DEFINE_CLASS(ABillboardActor, AActor)
REGISTER_FACTORY(ABillboardActor)

DEFINE_CLASS(ADecalActor, AActor)
REGISTER_FACTORY(ADecalActor)

DEFINE_CLASS(AFireBallActor, AActor)
REGISTER_FACTORY(AFireBallActor)


DEFINE_CLASS(AHeightFogActor, AActor)
REGISTER_FACTORY(AHeightFogActor)

//void ACubeActor::InitDefaultComponents()
//{
//	auto* Cube = AddComponent<UStaticMeshComponent>();
//	Cube->SetStaticMesh(FResourceManager::Get().LoadStaticMesh(CubeMeshPath));
//	SetRootComponent(Cube);
//
//	// Text
//	UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
//	Text->SetFont(FName("Default"));
//	Text->AttachToComponent(Cube);
//	Text->SetText("UUID: " + std::to_string(GetUUID()));
//	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
//
//	// SubUV
//	USubUVComponent* SubUV = AddComponent<USubUVComponent>();
//	SubUV->AttachToComponent(Cube);
//	SubUV->SetParticle(FName("Explosion"));
//	SubUV->SetSpriteSize(2.0f, 2.0f);
//	SubUV->SetFrameRate(30.f);
//	SubUV->SetRelativeLocation(FVector(0.0f, 0.0f, 2.3f));
//}
//
//void ASphereActor::InitDefaultComponents()
//{
//	auto* Sphere = AddComponent<UStaticMeshComponent>();
//	Sphere->SetStaticMesh(FResourceManager::Get().LoadStaticMesh(SphereMeshPath));
//	SetRootComponent(Sphere);
//
//	UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
//	Text->SetFont(FName("Default"));
//	Text->AttachToComponent(Sphere);
//	Text->SetText("UUID: " + std::to_string(GetUUID()));
//	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
//
//	// SubUV
//	USubUVComponent* SubUV = AddComponent<USubUVComponent>();
//	SubUV->AttachToComponent(Sphere);
//	SubUV->SetParticle(FName("Explosion"));
//	SubUV->SetSpriteSize(2.0f, 2.0f);
//	SubUV->SetFrameRate(30.f);
//	SubUV->SetRelativeLocation(FVector(0.0f, 0.0f, 2.3f));
//}
//
//void APlaneActor::InitDefaultComponents()
//{
//	auto* Plane = AddComponent<UStaticMeshComponent>();
//	Plane->SetStaticMesh(FResourceManager::Get().LoadStaticMesh(PlaneMeshPath));
//	SetRootComponent(Plane);
//
//	UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
//	Text->SetFont(FName("Default"));
//	Text->SetText(std::format("UUID: {}", GetUUID()));
//	Text->AttachToComponent(Plane);
//	Text->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
//
//	// SubUV
//	USubUVComponent* SubUV = AddComponent<USubUVComponent>();
//	SubUV->AttachToComponent(Plane);
//	SubUV->SetParticle(FName("Explosion"));
//	SubUV->SetSpriteSize(2.0f, 2.0f);
//	SubUV->SetFrameRate(30.f);
//	SubUV->SetRelativeLocation(FVector(0.0f, 0.0f, 2.3f));
//}
DEFINE_CLASS(ASpotLightActor, AActor)
REGISTER_FACTORY(ASpotLightActor)

void AStaticMeshActor::InitDefaultComponents()
{
	auto* StaticMesh = AddComponent<UStaticMeshComponent>();;
	SetRootComponent(StaticMesh);

#ifndef NO_UUID
	auto* Text = AddComponent<UTextRenderComponent>();
	Text->AttachToComponent(StaticMesh);
	Text->SetFont(FName("Default"));
	Text->SetText("UUID: " + std::to_string(GetUUID()));
	FVector Extent = StaticMesh->GetWorldAABB().GetExtent();
	Text->SetRelativeLocation(FVector(0.0f, 0.0f, Extent.Z * 2.0f));
#endif
}

void ASubUVActor::InitDefaultComponents()
{
    auto* SubUV = AddComponent<USubUVComponent>();
    SetRootComponent(SubUV);
	SubUV->SetParticle(FName("Explosion"));
	SubUV->SetSpriteSize(2.0f, 2.0f);
	SubUV->SetFrameRate(30.f);

    auto* Text = AddComponent<UTextRenderComponent>();
    Text->AttachToComponent(SubUV);
    Text->SetFont(FName("Default"));
    Text->SetText("UUID: " + std::to_string(GetUUID()));

    FVector Extent = SubUV->GetWorldAABB().GetExtent();
    Text->SetRelativeLocation(FVector(0.0f, 0.0f, Extent.Y * 1.4f));
}

void ATextRenderActor::InitDefaultComponents()
{
	UTextRenderComponent* Text = AddComponent<UTextRenderComponent>();
	SetRootComponent(Text);
	Text->SetFont(FName("Default"));
	Text->SetText("TextRender");

    auto* TextUUID = AddComponent<UTextRenderComponent>();
    TextUUID->AttachToComponent(Text);
    TextUUID->SetFont(FName("Default"));
    TextUUID->SetText("UUID: " + std::to_string(GetUUID()));

    FVector Extent = TextUUID->GetWorldAABB().GetExtent();
    TextUUID->SetRelativeLocation(FVector(0.0f, 0.0f, Extent.Y * 0.6f));
}

void ABillboardActor::InitDefaultComponents()
{
	UBillboardComponent* Billboard = AddComponent<UBillboardComponent>();
	SetRootComponent(Billboard);
	Billboard->SetTextureName(("Asset\\Texture\\Pawn_64x.png"));
	//Billboard->SetTextureName();

    auto* TextUUID = AddComponent<UTextRenderComponent>();
    TextUUID->AttachToComponent(Billboard);
    TextUUID->SetFont(FName("Default"));
    TextUUID->SetText("UUID: " + std::to_string(GetUUID()));

    FVector Extent = TextUUID->GetWorldAABB().GetExtent();
    TextUUID->SetRelativeLocation(FVector(0.0f, 0.0f, Extent.Y * 0.6f));
}

void ADecalActor::InitDefaultComponents()
{
	DecalComponent = AddComponent<UDecalComponent>();
	DecalComponent->SetDecalTexturePath("Asset\\Texture\\milky way.png");
	SetRootComponent(DecalComponent);

	IconBillboardComponent = AddComponent<UBillboardComponent>();
	IconBillboardComponent->SetEditorOnly(true);
	IconBillboardComponent->SetTextureName(("Asset\\Texture\\S_DecalActorIcon.png"));
	IconBillboardComponent->AttachToComponent(DecalComponent);
}

void ASpotLightActor::InitDefaultComponents()
{
	constexpr int32 SpotLightDecalStepCount = 10;
	constexpr float SpotLightMaxDistance = 4.0f;
	constexpr float SpotLightDecalStepLength = SpotLightMaxDistance / SpotLightDecalStepCount;
	constexpr float SpotLightDecalHalfDepth = SpotLightDecalStepLength * 0.5f;
	constexpr float SpotLightNearHalfSize = 0.1f;
	constexpr float SpotLightFarHalfSize = 2.0f;

	IconBillboardComponent = AddComponent<UBillboardComponent>();
	IconBillboardComponent->SetTextureName(("Asset\\Texture\\SpotLight_64x.png"));
	SetRootComponent(IconBillboardComponent);

	for (int32 StepIndex = 0; StepIndex < SpotLightDecalStepCount; ++StepIndex)
	{
		UDecalComponent* StepDecalComponent = AddComponent<UDecalComponent>();
		StepDecalComponent->SetDecalTexturePath("Asset\\Texture\\SpotLightDecal.png");
		StepDecalComponent->AttachToComponent(IconBillboardComponent);

		const float StepAlpha = (SpotLightDecalStepCount > 1)
			? static_cast<float>(StepIndex) / static_cast<float>(SpotLightDecalStepCount - 1)
			: 0.0f;
		const float HalfWidth = SpotLightNearHalfSize + ((SpotLightFarHalfSize - SpotLightNearHalfSize) * StepAlpha);
		const float Distance = SpotLightDecalHalfDepth + (SpotLightDecalStepLength * static_cast<float>(StepIndex));

		StepDecalComponent->SetRelativeScale(FVector(SpotLightDecalHalfDepth, HalfWidth, HalfWidth));
		StepDecalComponent->SetRelativeLocation(FVector(Distance, 0.0f, 0.0f));
	}

	SetActorRotation(FVector(0.0f, 90.0f, 0.0f));
}

void AFireBallActor::InitDefaultComponents()
{
	FireBallComponent = AddComponent<UFireBallComponent>();
	SetRootComponent(FireBallComponent);
	
}


void AHeightFogActor::InitDefaultComponents()
{
	HeightFogComponent = AddComponent<UHeightFogComponent>();
	SetRootComponent(HeightFogComponent);
	
}