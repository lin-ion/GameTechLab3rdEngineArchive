#include "pch.h"
#include "World.h"

#include "ResourceManager.h"
#include "ObjectFactory.h"
#include "Level.h"
#include "Actor.h"
#include "Object.h"

#include "CubeComponent.h"
#include "GizmoActor.h"
#include "ArrowComponent.h"

#include "PrimitiveComponent.h"
#include "SphereComponent.h"

#include "ImGuiDrawer.h"
#include "FEditorViewportClient.h"

#include "Input.h"


void UWorld::InitWorld(UResourceManager& ResourceManager, FEditorViewportClient* _ViewPort)
{
	CurrentLevel = UObjectFactory::NewObject<ULevel>();
    AActor* GizmoStorageActor = SpawnActor<AActor>();

	CurrentLevel->OwningWorld = this;
	ViewPort = _ViewPort;


	resourceManager = &ResourceManager;

	AActor* CubeActor = SpawnActor<AActor>();
	UGizmoActor* GizmoActor = SpawnActor<UGizmoActor>();


	//레벨에 엑터 추가
	UCubeComponent* CubeComponent = CubeActor->AddComponent<UCubeComponent>();
	CubeComponent->SetMesh(ResourceManager.FindMeshData("Cube"));
	CubeComponent->SetHovering(true);
	CubeActor->RootComponent = CubeComponent;


	GizmoActor->ArrowY->SetMesh(ResourceManager.FindMeshData("Gizmo"));
	GizmoActor->ArrowX->SetMesh(ResourceManager.FindMeshData("Gizmo"));
	GizmoActor->ArrowZ->SetMesh(ResourceManager.FindMeshData("Gizmo"));

	GizmoActor->BasePoint->SetMesh(ResourceManager.FindMeshData("Sphere"));

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

		// 2순위: 기즈모가 아니라면 월드 공간의 액터를 피킹합니다.
		AActor* HitActor = GetPickedActor();

		// 3순위: 액터가 선택되었다면 기즈모를 해당 액터로 이사시킵니다.
		if (HitActor)
		{
			//TransferGizmo(HitActor);
		}
	}
}

// 가장 가까운 것만 골라가도록 추후 수정
AActor* UWorld::GetPickedActor()
{
	auto ActorArray = CurrentLevel->Actors;

	for (size_t ActorIndex = 0; ActorIndex < ActorArray.Size(); ++ActorIndex)
	{
		AActor* TargetActor = ActorArray[ActorIndex];
		// [핵심 방어] 기즈모를 담고 있는 액터 자체는 피킹 검수에서 제외합니다.
		//if (MainGizmo == TargetActor) continue;

		TArray<UPrimitiveComponent*> PrimitiveComponents = TargetActor->GetComponentArrayByClass<UPrimitiveComponent>();

		for (size_t ComponentIndex = 0; ComponentIndex < PrimitiveComponents.Size(); ++ComponentIndex)
		{
			UPrimitiveComponent* Primitive = PrimitiveComponents[ComponentIndex];
			if (!Primitive) continue;

			FMatrix ModelWorld = Primitive->GetComponentTransform();
			FVector _CameraPos = FMatrix::TransformCoord(ViewPort->GetViewLocation(), ModelWorld.Inverse());
			FVector _CameraRay = FMatrix::TransformNormal(ViewPort->GetCameraRayDirection(), ModelWorld.Inverse());

			const UMesh* MeshData = Primitive->GetMesh();
			const FVertexSimple* BufferData = static_cast<const FVertexSimple*>(Primitive->GetMesh()->GetVertexData());

			for (uint64 vertexIndex = 0; vertexIndex < MeshData->GetVertexCount(); vertexIndex += 3)
			{
				if (RayIntersectsTriangle(_CameraPos, _CameraRay, BufferData[vertexIndex], BufferData[vertexIndex + 1], BufferData[vertexIndex + 2]))
				{
					PickedActor = TargetActor;
					return TargetActor;
				}
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
