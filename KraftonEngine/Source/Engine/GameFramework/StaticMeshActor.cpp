#include "GameFramework/StaticMeshActor.h"
#include "Object/ObjectFactory.h"
#include "Engine/Runtime/Engine.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/BillboardComponent.h"
#include "Texture/Texture2D.h"

IMPLEMENT_CLASS(AStaticMeshActor, AActor)

void AStaticMeshActor::InitDefaultComponents(const FString& UStaticMeshFileName)
{
	StaticMeshComponent = AddComponent<UStaticMeshComponent>();
	SetRootComponent(StaticMeshComponent);

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	UStaticMesh* Asset = FObjManager::LoadObjStaticMesh(UStaticMeshFileName, Device);

	StaticMeshComponent->SetStaticMesh(Asset);

	// UUID 텍스트 표시
	TextRenderComponent = AddComponent<UTextRenderComponent>();
	TextRenderComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 1.3f));
	TextRenderComponent->SetText("UUID : " + TextRenderComponent->GetOwnerUUIDToString());
	TextRenderComponent->AttachToComponent(StaticMeshComponent);
	TextRenderComponent->SetFont(FName("Default"));

	// Pawn 아이콘 빌보드
	BillboardComponent = AddComponent<UBillboardComponent>();
	BillboardComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 2.0f));
	BillboardComponent->AttachToComponent(StaticMeshComponent);
	UTexture2D* PawnIcon = UTexture2D::LoadFromFile("Asset/Editor/Icon/Pawn_64x.png", Device);
	BillboardComponent->SetSprite(PawnIcon);
}