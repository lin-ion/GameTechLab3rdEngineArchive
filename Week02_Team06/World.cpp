#include "pch.h"
#include "World.h"

#include "ResourceManager.h"
#include "ObjectFactory.h"
#include "Level.h"
#include "Actor.h"
#include "Object.h"
#include "PrimitiveComponent.h"
#include "CubeComponent.h"


void UWorld::InitWorld(UResourceManager& ResourceManager)
{
	CurrentLevel = UObjectFactory::NewObject<ULevel>();
    AActor* GizmoStorageActor = SpawnActor<AActor>();
    MainGizmo = GizmoStorageActor->AddComponent<UGizmoComponent>();
	AActor* CubeActor = SpawnActor<AActor>();

	//레벨에 엑터 추가
	UCubeComponent* CubeComponent = CubeActor->AddComponent<UCubeComponent>();
	CubeComponent->SetMesh(ResourceManager.FindMeshData("Cube"));
	CubeComponent->SetPosition({ 0.0f, 0.0f, 3.0f }); // 카메라 앞에 배치
	CubeComponent->SetScale({ 0.5f, 0.5f, 0.5f });
}

void UWorld::Tick(float DeltaTime)
{
	if (!CurrentLevel) return;

	for (uint32 i = 0; i < CurrentLevel->Actors.Size(); ++i)
	{
		CurrentLevel->Actors[i]->Tick(DeltaTime);
	}
}

void UWorld::PickActor(const FVector& RayOrigin, const FVector& RayDir)
{
    // 1. 기즈모 우선 검수: 지금 선택된 애가 있고, 기즈모가 붙어있다면 기즈모부터 확인!
    if (SelectedActor && MainGizmo)
    {
        // 기즈모는 특수 판정(CheckGizmoPicking)을 사용합니다.
        if (MainGizmo->CheckGizmoPicking(nullptr) != EGizmoAxis::None) return;
    }

    // 2. 월드 전수 조사: 모든 액터의 모든 삼각형을 검수합니다.
    AActor* BestHitActor = nullptr;
    float ClosestDist = FLT_MAX;

    for (uint32 i = 0; i < CurrentLevel->Actors.Size(); ++i)
    {
        AActor* Actor = CurrentLevel->Actors[i];
        if (Actor == MainGizmo->GetOwner()) continue; // 기즈모 보관용 액터는 제외

        UPrimitiveComponent* Primitive = Actor->GetComponentByClass<UPrimitiveComponent>();
        if (!Primitive) continue;

        // TODO: 여기서 삼각형 피킹 로직(PerformPicking)을 수행하여 BestHitActor를 결정합니다.
    }

    // 3. 기즈모 배차: 새로운 액터가 선택되었다면 기즈모를 보냅니다.
    if (BestHitActor) TransferGizmo(BestHitActor);
}

void UWorld::Release()
{
	// World는 참조만 정리
	CurrentLevel = nullptr;
}

void UWorld::TransferGizmo(AActor* NewTarget)
{
    if (SelectedActor == NewTarget) return;

    // [전출] 기존 주인에게서 기즈모 회수 (RemoveSwap 공정)
    if (SelectedActor && MainGizmo)
    {
        SelectedActor->RemoveComponent(MainGizmo);
    }

    SelectedActor = NewTarget;

    if (SelectedActor && MainGizmo)
    {
        // [위치 동기화] SceneComponent(Primitive)의 위치를 기즈모에 세팅합니다.
        UPrimitiveComponent* TargetVisual = SelectedActor->GetComponentByClass<UPrimitiveComponent>();
        if (TargetVisual)
        {
            // Actor에는 Position이 없으므로 Component의 좌표를 사용합니다.
            MainGizmo->SetPosition(TargetVisual->GetComponentLocation());
        }

        // [전입] 새 주인에게 기즈모 장착 (인스턴스 기반 Add)
        SelectedActor->AddComponent(MainGizmo);
    }
}