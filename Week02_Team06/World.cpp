#include "pch.h"
#include "World.h"

#include "ResourceManager.h"
#include "ObjectFactory.h"
#include "Level.h"
#include "Actor.h"
#include "Object.h"
#include "PrimitiveComponent.h"
#include "CubeComponent.h"
#include "ImGuiDrawer.h"
#include "FEditorViewportClient.h"

#include "GizmoComponent.h"
#include "Input.h"


void UWorld::InitWorld(UResourceManager& ResourceManager, FEditorViewportClient* _ViewPort)
{
	CurrentLevel = UObjectFactory::NewObject<ULevel>();
    AActor* GizmoStorageActor = SpawnActor<AActor>();
    MainGizmo = GizmoStorageActor->AddComponent<UGizmoComponent>();
	CurrentLevel->OwningWorld = this;
	ViewPort = _ViewPort;


	resourceManager = &ResourceManager;

	AActor* CubeActor = SpawnActor<AActor>();

	//레벨에 엑터 추가
	UCubeComponent* CubeComponent = CubeActor->AddComponent<UCubeComponent>();
	CubeComponent->SetMesh(ResourceManager.FindMeshData("Cube"));
	CubeComponent->SetHovering(true);
	CubeActor->RootComponent = CubeComponent;

	//RTTI 테스트
	if (CubeComponent->IsA(UPrimitiveComponent::StaticClass()))
	{
		int iDebug = 0;
	}

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

	// [공정 연동] 마우스 좌클릭 시 피킹 파이프라인 가동
	if (UInput::GetInstance().IsKeyDown(VK_LBUTTON))
	{
		// 1순위: 기즈모 축을 잡았는지 먼저 확인합니다.
		if (SelectedActor && MainGizmo)
		{
			EGizmoAxis PickedAxis = MainGizmo->CheckGizmoPicking(nullptr);
			if (PickedAxis != EGizmoAxis::None)
			{
				// 기즈모를 클릭했다면 월드 피킹을 생략하고 빠져나갑니다.
				// (다음 단계에서 여기에 '축을 따라 물체 이동하는 로직'을 연결할 것입니다.)
				return;
			}
		}

		// 2순위: 기즈모가 아니라면 월드 공간의 액터를 피킹합니다.
		AActor* HitActor = GetPickedActor();

		// 3순위: 액터가 선택되었다면 기즈모를 해당 액터로 이사시킵니다.
		if (HitActor)
		{
			TransferGizmo(HitActor);
		}
	}
}

//가장 가까운 것만 골라가도록 추후 수정
AActor* UWorld::GetPickedActor()
{
	auto ActorArray = CurrentLevel->Actors;

	for (size_t i = 0; i < ActorArray.Size(); ++i)
	{
		AActor* TargetActor = ActorArray[i];

		// [핵심 방어] 기즈모를 담고 있는 액터 자체는 피킹 검수에서 제외합니다.
		if (MainGizmo && TargetActor == MainGizmo->GetOwner()) continue;

		UPrimitiveComponent* Primitive = TargetActor->GetComponentByClass<UPrimitiveComponent>();
		// [수정 완료] return nullptr; 가 아니라 continue; 로 다음 액터를 검사해야 합니다.
		if (!Primitive) continue;

		FMatrix ModelWorld = TargetActor->RootComponent->GetComponentTransform();
		FVector _CameraPos = FMatrix::TransformCoord(ViewPort->GetViewLocation(), ModelWorld.Inverse());
		FVector _CameraRay = FMatrix::TransformNormal(ViewPort->GetCameraRayDirection(), ModelWorld.Inverse());

		const UMesh* MeshData = Primitive->GetMesh();
		const FVertexSimple* BufferData = static_cast<const FVertexSimple*>(Primitive->GetMesh()->GetVertexData());

		for (int32 vertexIndex = 0; vertexIndex < MeshData->GetVertexCount(); vertexIndex += 3)
		{
			if (RayIntersectsTriangle(_CameraPos, _CameraRay, BufferData[vertexIndex], BufferData[vertexIndex + 1], BufferData[vertexIndex + 2]))
			{
				PickedActor = TargetActor;
				return TargetActor;
			}
		}
	}

	return nullptr;
}

bool UWorld::RayIntersectsTriangle(const FVector& CameraPos, const FVector& CameraRay, const FVertexSimple& V0, const FVertexSimple& V1, const FVertexSimple& V2)
{

	FVector Vertex0 = { V0.X, V0.Y, V0.Z };
	FVector Vertex1 = { V1.X, V1.Y, V1.Z };
	FVector Vertex2 = { V2.X, V2.Y, V2.Z };

	FVector Edge1 = Vertex1 - Vertex0;
	FVector Edge2 = Vertex2 - Vertex0;

	FVector Normal = Edge1.FVector::Cross(Edge2);

	//후면인지 판단
	if (Normal.FVector::Dot(CameraRay) > 0)
	{
		return false;
	}
	constexpr float epsilon = std::numeric_limits <float> ::epsilon();

	// det 계산 — 0이면 광선이 삼각형 평면과 평행
	FVector RayCrossE2 = CameraRay.Cross(Edge2);
	float   Det = Edge1.Dot(RayCrossE2);
	if (abs(Det) < epsilon) return false;

	float   InvDet = 1.f / Det;
	FVector S = CameraPos - Vertex0;

	// u 범위 체크
	float U = InvDet * S.Dot(RayCrossE2);
	if (U < 0.f || U > 1.f) return false;

	// v 범위 체크
	FVector SCrossE1 = S.Cross(Edge1);
	float   V = InvDet * CameraRay.Dot(SCrossE1);
	if (V < 0.f || U + V > 1.f) return false;

	// t > 0 이면 광선 방향 앞쪽에 교차점 존재
	float T = InvDet * Edge2.Dot(SCrossE1);
	//위치 겟ㄴ은 나온 T를 이용하여 Origin + t * Ray방향벡터로 계산
	return T > epsilon;
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

void UWorld::SpawnActorFromEditor(FSpawnParameters params)
{
	for (int i = 0; i < params.Count; i++)
	{
		AActor* actor = SpawnActor<AActor>();

		if (params.PrimitiveType == "Cube")
		{
			UCubeComponent* Cube = actor->AddComponent<UCubeComponent>();
			Cube->SetMesh(resourceManager->FindMeshData("Cube"));
			actor->RootComponent = Cube;
		}
		else
		{
			return;
		}

		actor->RootComponent->SetPosition(params.Location);
		actor->RootComponent->SetRotation(params.Rotation);
		actor->RootComponent->SetScale(params.Scale);
	}
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