#include "pch.h"
#include "World.h"

#include "ResourceManager.h"
#include "ObjectFactory.h"
#include "Level.h"
#include "Actor.h"
#include "Object.h"

#include "CubeComponent.h"
#include "ArrowComponent.h"
#include "RingComponent.h"
#include "LocationGizmoActor.h"
#include "RotationGizmoActor.h"

#include "PrimitiveComponent.h"
#include "SphereComponent.h"

#include "ImGuiDrawer.h"
#include "FEditorViewportClient.h"

#include "Input.h"
#include "Mesh.h"



void UWorld::InitWorld(UResourceManager& ResourceManager, FEditorViewportClient* _ViewPort)
{
	CurrentLevel = UObjectFactory::NewObject<ULevel>();
	CurrentLevel->OwningWorld = this;
	ViewPort = _ViewPort;

	resourceManager = &ResourceManager;

	AActor* CubeActor = SpawnActor<AActor>();
	LocationGizmoActor = SpawnActor<ULocationGizmoActor>();
	RotationGizmoActor = SpawnActor<URotationGizmoActor>();

	//레벨에 엑터 추가
	UCubeComponent* CubeComponent = CubeActor->AddComponent<UCubeComponent>();

	CubeComponent->SetMesh(ResourceManager.FindMeshData("Cube"));
	CubeComponent->SetHovering(true);
	CubeActor->RootComponent = CubeComponent;
	CubeActor->BoundingSphere->SetScale(CubeActor->RootComponent->GetScale() * 1.5f);

	RotationGizmoActor->RingY->SetMesh(ResourceManager.FindMeshData("GizmoRotation"));
	RotationGizmoActor->RingX->SetMesh(ResourceManager.FindMeshData("GizmoRotation"));
	RotationGizmoActor->RingZ->SetMesh(ResourceManager.FindMeshData("GizmoRotation"));

	LocationGizmoActor->ArrowY->SetMesh(ResourceManager.FindMeshData("GizmoLocation"));
	LocationGizmoActor->ArrowX->SetMesh(ResourceManager.FindMeshData("GizmoLocation"));
	LocationGizmoActor->ArrowZ->SetMesh(ResourceManager.FindMeshData("GizmoLocation"));
	LocationGizmoActor->BasePoint->SetMesh(ResourceManager.FindMeshData("Sphere"));
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

	UInput& Input = UInput::GetInstance();

	// 모드 전환 스위치 (Z: 이동, X: 회전)
	if (Input.IsKeyDown('Z')) CurrentMode = EGizmoMode::Location;
	if (Input.IsKeyDown('X')) CurrentMode = EGizmoMode::Rotation;

	PreparePicking();

	for (uint32 i = 0; i < CurrentLevel->Actors.Size(); ++i)
	{
		CurrentLevel->Actors[i]->Tick(DeltaTime);
	}

	FVector2D ScreenPos = { static_cast<float>(Input.GetMousePosition().x), static_cast<float>(Input.GetMousePosition().y) };
	FVector RayDirection;
	FVector RayOrigin;
	ViewPort->DeprojectScreenToWorld(ScreenPos, RayOrigin, RayDirection);

	// 드래그 시작 (초기 상태 저장)
	if (Input.IsKeyDown(VK_LBUTTON) && HoveredAxis != EGizmoAxis::None)
	{
		bIsDragging = true;
		CurrentDraggingAxis = HoveredAxis;

		if (CurrentMode == EGizmoMode::Location && LocationGizmoActor)
		{
			GizmoStartLocation = LocationGizmoActor->RootComponent->GetPosition();
			DragStartPoint = LocationGizmoActor->GetDragIntersectionPoint(RayOrigin, RayDirection, CurrentDraggingAxis);
		}
		else if (CurrentMode == EGizmoMode::Rotation && RotationGizmoActor)
		{
			// 타겟과 기즈모의 초기 회전값 각각 저장
			if (SelectedActor) TargetStartRotation = SelectedActor->RootComponent->GetRotation();
			GizmoStartRotation = RotationGizmoActor->RootComponent->GetRotation();

			// 드래그 시작 순간, 링의 방향을 찰칵! 고정시킵니다.
			RotationGizmoActor->LockDragPlane(CurrentDraggingAxis);

			// 이후 교차점 계산
			DragStartPoint = RotationGizmoActor->GetDragIntersectionPoint(RayOrigin, RayDirection, CurrentDraggingAxis);
		}
	}

	// 드래그 진행 중
	if (bIsDragging && Input.IsKeyPressing(VK_LBUTTON))
	{
		if (CurrentMode == EGizmoMode::Location && LocationGizmoActor)
		{
			FVector CurrentPoint = LocationGizmoActor->GetDragIntersectionPoint(RayOrigin, RayDirection, CurrentDraggingAxis);
			FVector Delta = CurrentPoint - DragStartPoint;
			LocationGizmoActor->RootComponent->SetPosition(GizmoStartLocation + Delta);
		}
		else if (CurrentMode == EGizmoMode::Rotation && RotationGizmoActor)
		{
			FVector CurrentPoint = RotationGizmoActor->GetDragIntersectionPoint(RayOrigin, RayDirection, CurrentDraggingAxis);

			// 🚨 로드리게스 원리로 추출한 단일 회전 각도(float)를 받아옵니다.
			float DeltaAngle = RotationGizmoActor->GetRotationDelta(CurrentPoint, DragStartPoint, CurrentDraggingAxis);

			// 1. 타겟 액터 회전 (선택된 축에만 각도 더하기)
			if (SelectedActor)
			{
				FVector TargetDelta = FVector::Zero;
				if (CurrentDraggingAxis == EGizmoAxis::X) TargetDelta.X = DeltaAngle;
				if (CurrentDraggingAxis == EGizmoAxis::Y) TargetDelta.Y = DeltaAngle;
				if (CurrentDraggingAxis == EGizmoAxis::Z) TargetDelta.Z = DeltaAngle;

				SelectedActor->RootComponent->SetRotation(TargetStartRotation + TargetDelta);
			}

			// 2. 기즈모 전체가 아닌, '잡고 있는 링' 하나만 제자리에서 돌립니다!
			RotationGizmoActor->ApplyRingRotation(CurrentDraggingAxis, DeltaAngle);
		}
	}
	

	if (Input.IsKeyUp(VK_LBUTTON))
	{
		bIsDragging = false;
		CurrentDraggingAxis = EGizmoAxis::None;

		// 마우스를 놓으면 링의 시각적 회전을 원래 예쁜 구(Sphere) 형태로 복구
		if (RotationGizmoActor)
		{
			RotationGizmoActor->ApplyRingRotation(EGizmoAxis::None, 0.0f);
		}
	}
}

void UWorld::SpawnActorFromEditor(FSpawnParameters params)
{
	for (int i = 0; i < params.Count; i++)
	{
		AActor* actor = SpawnActor<AActor>();
		if (params.bOverrideUUID)
			actor->SceneUUID = params.UUID;
		else
			actor->SceneUUID = UEngineStatics::GetSceneUUID();

		if (params.PrimitiveType == "Cube")
		{
			UCubeComponent* Cube = actor->AddComponent<UCubeComponent>();
			Cube->SetMesh(resourceManager->FindMeshData(params.PrimitiveType));
			actor->RootComponent = Cube;
		}
		else
		{
			continue;
		}

		actor->RootComponent->SetPosition(params.Location);
		actor->RootComponent->SetRotation(params.Rotation);
		actor->RootComponent->SetScale(params.Scale);
	}
}

void UWorld::PreparePicking()
{
	HoveredAxis = EGizmoAxis::None;
	if (bIsDragging) return;

	if (CurrentMode == EGizmoMode::Location && LocationGizmoActor)
	{
		HoveredAxis = LocationGizmoActor->CheckGizmoPicking();
	}
	else if (CurrentMode == EGizmoMode::Rotation && RotationGizmoActor)
	{
		HoveredAxis = RotationGizmoActor->CheckGizmoPicking();
	}
}

// 가장 가까운 것만 골라가도록 추후 수정
AActor* UWorld::GetPickedActor()
{
	auto ActorArray = CurrentLevel->Actors;

	for (size_t ActorIndex = 0; ActorIndex < ActorArray.Size(); ++ActorIndex)
	{
		AActor* TargetActor = ActorArray[ActorIndex];
		// 기즈모를 담고 있는 액터 자체는 피킹 검수에서 제외합니다.
		if (LocationGizmoActor == TargetActor || RotationGizmoActor == TargetActor) continue;

		TArray<UPrimitiveComponent*> PrimitiveComponents = TargetActor->GetComponentArrayByClass<UPrimitiveComponent>();

		for (size_t ComponentIndex = 0; ComponentIndex < PrimitiveComponents.Size(); ++ComponentIndex)
		{
			UPrimitiveComponent* Primitive = PrimitiveComponents[ComponentIndex];
			if (!Primitive) continue;

			FMatrix ModelWorld = Primitive->GetComponentTransform();
			FVector _CameraPos = FMatrix::TransformCoord(ViewPort->GetViewLocation(), ModelWorld.Inverse());
			FVector _CameraRay = FMatrix::TransformNormal(ViewPort->GetCameraRayDirection(), ModelWorld.Inverse());

			const UMesh* MeshData = Primitive->GetMesh();
			const FVertexSimple* BufferData = Primitive->GetMesh()->GetVertexData();

			for (uint64 vertexIndex = 0; vertexIndex < MeshData->GetVertexCount(); vertexIndex += 3)
			{
				FVector V0 = { BufferData[vertexIndex].X,     BufferData[vertexIndex].Y,     BufferData[vertexIndex].Z };
				FVector V1 = { BufferData[vertexIndex + 1].X, BufferData[vertexIndex + 1].Y, BufferData[vertexIndex + 1].Z };
				FVector V2 = { BufferData[vertexIndex + 2].X, BufferData[vertexIndex + 2].Y, BufferData[vertexIndex + 2].Z };

				if (Math::RayIntersectsTriangle(_CameraPos, _CameraRay, V0, V1, V2))
				{
					return TargetActor;
				}
			}
		}
	}

	return nullptr;
}

// 공용 매시 충돌검사 함수
bool UWorld::RayIntersectsMesh(const FVector& RayOrigin, const FVector& RayDir, const UMesh* Mesh, const FMatrix& WorldMatrix)
{
	if (!Mesh) return false;

	FVector _CameraPos = FMatrix::TransformCoord(RayOrigin, WorldMatrix.Inverse());
	FVector _CameraRay = FMatrix::TransformNormal(RayDir, WorldMatrix.Inverse());

	const FVertexSimple* BufferData = static_cast<const FVertexSimple*>(Mesh->GetVertexData());

	for (uint64 i = 0; i < Mesh->GetVertexCount(); i += 3)
	{
		FVector V0 = { BufferData[i].X,     BufferData[i].Y,     BufferData[i].Z };
		FVector V1 = { BufferData[i + 1].X, BufferData[i + 1].Y, BufferData[i + 1].Z };
		FVector V2 = { BufferData[i + 2].X, BufferData[i + 2].Y, BufferData[i + 2].Z };

		if (Math::RayIntersectsTriangle(_CameraPos, _CameraRay, V0, V1, V2))
		{
			return true;
		}
	}
	return false;
}

void UWorld::Release()
{
	// World는 참조만 정리
	CurrentLevel = nullptr;
}

void UWorld::ClearScene()
{
	for (size_t i = 0; i < CurrentLevel->Actors.Size(); ++i)
	{
		UObjectFactory::DestroyObject(CurrentLevel->Actors[i]);
	}

	CurrentLevel->Actors.Clear();
	UEngineStatics::SceneUUID = 0;
}

