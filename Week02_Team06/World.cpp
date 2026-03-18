#include "pch.h"
#include "World.h"

#include "ResourceManager.h"
#include "ObjectFactory.h"
#include "Level.h"
#include "Actor.h"
#include "Object.h"

#include "LocationGizmoActor.h"
#include "RotationGizmoActor.h"
#include "ScaleGizmoActor.h"

#include "PrimitiveComponent.h"
#include "SphereComponent.h"
#include "CubeComponent.h"
#include "ArrowComponent.h"
#include "RingComponent.h"
#include "HammerComponent.h"

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
	ScaleGizmoActor = SpawnActor<UScaleGizmoActor>();

	//레벨에 엑터 추가
	UCubeComponent* CubeComponent = CubeActor->AddComponent<UCubeComponent>();


	CubeComponent->SetMesh(ResourceManager.FindMeshData("Cube"));

	CubeActor->RootComponent = CubeComponent;
	//CubeActor->BoundingSphere->SetScale(CubeActor->RootComponent->GetScale() * 1.5f);

	RotationGizmoActor->RingY->SetMesh(ResourceManager.FindMeshData("GizmoRotation"));
	RotationGizmoActor->RingX->SetMesh(ResourceManager.FindMeshData("GizmoRotation"));
	RotationGizmoActor->RingZ->SetMesh(ResourceManager.FindMeshData("GizmoRotation"));

	LocationGizmoActor->ArrowY->SetMesh(ResourceManager.FindMeshData("GizmoLocation"));
	LocationGizmoActor->ArrowX->SetMesh(ResourceManager.FindMeshData("GizmoLocation"));
	LocationGizmoActor->ArrowZ->SetMesh(ResourceManager.FindMeshData("GizmoLocation"));
	LocationGizmoActor->BasePoint->SetMesh(ResourceManager.FindMeshData("Sphere"));

	ScaleGizmoActor->HammerX->SetMesh(ResourceManager.FindMeshData("GizmoScale"));
	ScaleGizmoActor->HammerY->SetMesh(ResourceManager.FindMeshData("GizmoScale"));
	ScaleGizmoActor->HammerZ->SetMesh(ResourceManager.FindMeshData("GizmoScale"));
	ScaleGizmoActor->BasePoint->SetMesh(ResourceManager.FindMeshData("Cube"));

	if (CubeComponent->IsA(UPrimitiveComponent::StaticClass()))
	{
		int iDebug = 0;
	}

	CubeComponent->SetPosition({ 0.0f, 0.0f, 3.0f }); // 카메라 앞에 배치
	CubeComponent->SetScale({ 0.5f, 0.5f, 0.5f });

	SelectedActor = nullptr;
}

void UWorld::Tick(float DeltaTime)
{
	if (!CurrentLevel) return;

	UInput& Input = UInput::GetInstance();

	// 모드 전환 스위치 (Z: 이동, X: 회전, C: 스케일)
	if (Input.IsKeyDown('Z')) CurrentMode = EGizmoMode::Location;
	if (Input.IsKeyDown('X')) CurrentMode = EGizmoMode::Rotation;
	if (Input.IsKeyDown('C')) CurrentMode = EGizmoMode::Scale;

	// 기즈모 축 피킹 수행 
	// TODO: 직관적인 이름으로 Rename 필요합니다
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
			LocationGizmoActor->LockDragPlane(RayOrigin);
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
		else if (CurrentMode == EGizmoMode::Scale && ScaleGizmoActor)
		{
			if (SelectedActor) TargetStartScale = SelectedActor->RootComponent->GetScale();
			DragStartPoint = ScaleGizmoActor->GetDragIntersectionPoint(RayOrigin, RayDirection, CurrentDraggingAxis);
		}
		bIsDragging = true;
		CurrentDraggingAxis = HoveredAxis;
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

			// 로드리게스 원리로 추출한 단일 회전 각도(float)를 받아옵니다.
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
		else if (CurrentMode == EGizmoMode::Scale && ScaleGizmoActor)
		{
			FVector CurrentPoint = ScaleGizmoActor->GetDragIntersectionPoint(RayOrigin, RayDirection, CurrentDraggingAxis);
			FVector Delta = CurrentPoint - DragStartPoint;

			if (SelectedActor)
			{
				FVector NewScale = TargetStartScale;
				// 마우스 이동량에 따른 감도 조절 (0.1f 정도가 적당합니다)
				float Sensitivity = 0.05f;

				if (CurrentDraggingAxis == EGizmoAxis::X) NewScale.X += Delta.X * Sensitivity;
				else if (CurrentDraggingAxis == EGizmoAxis::Y) NewScale.Y += Delta.Y * Sensitivity;
				else if (CurrentDraggingAxis == EGizmoAxis::Z) NewScale.Z += Delta.Z * Sensitivity;
				else if (CurrentDraggingAxis == EGizmoAxis::Center) // 전체(Uniform) 스케일링
				{
					// 대각선 이동량의 평균을 구하여 전체 스케일에 반영
					float UniformDelta = (Delta.X + Delta.Y + Delta.Z) * Sensitivity;
					NewScale = NewScale + FVector(UniformDelta, UniformDelta, UniformDelta);
				}

				// 0 이하로 작아져서 메쉬가 뒤집히는 것 방지 (방어 코드)
				if (NewScale.X < 0.01f) NewScale.X = 0.01f;
				if (NewScale.Y < 0.01f) NewScale.Y = 0.01f;
				if (NewScale.Z < 0.01f) NewScale.Z = 0.01f;

				SelectedActor->RootComponent->SetScale(NewScale);
			}
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

	// Gizmo를 조작중이 아닐 때만 액터 피킹 수행
	if (Input.IsKeyDown(VK_LBUTTON) && !bIsDragging)
	{
		AActor* PreviousSelectedActor = SelectedActor;
		SelectedActor = RaycastForActor(RayOrigin, RayDirection);

		if (PreviousSelectedActor == SelectedActor
			|| PreviousSelectedActor == nullptr && SelectedActor == nullptr)
		{
			// nothing to do
		}
		else if (PreviousSelectedActor && !SelectedActor)
		{
			// 기존 액터에서 선택 해제
			PreviousSelectedActor->SetSelected(false);
		}
		else if (!PreviousSelectedActor && SelectedActor)
		{
			// 새로운 액터 선택 시작
			SelectedActor->SetSelected(true);
		}
		else if (PreviousSelectedActor && SelectedActor)
		{
			PreviousSelectedActor->SetSelected(false);
			SelectedActor->SetSelected(true);
		}
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
	else if (CurrentMode == EGizmoMode::Scale && ScaleGizmoActor)
	{
		HoveredAxis = ScaleGizmoActor->CheckGizmoPicking();
	}
}

AActor* UWorld::RaycastForActor(const FVector& RayOrigin, const FVector& RayDirection)
{
	auto ActorArray = CurrentLevel->Actors;

	AActor* ClosestActor = nullptr;
	float MinT = 1e9;

	for (size_t ActorIndex = 0; ActorIndex < ActorArray.Size(); ++ActorIndex)
	{
		AActor* TargetActor = ActorArray[ActorIndex];
		// 기즈모를 담고 있는 액터 자체는 피킹 검수에서 제외합니다.
		if (LocationGizmoActor == TargetActor || RotationGizmoActor == TargetActor || ScaleGizmoActor == TargetActor) continue;

		// 구와 레이 충돌로 불필요한 정점 순회를 막음
		if (!RayIntersectsSphere(RayOrigin, RayDirection, TargetActor->RootComponent, TargetActor->RootComponent->GetComponentTransform()))
		{
			continue;
		}

		TArray<UPrimitiveComponent*> PrimitiveComponents = TargetActor->GetComponentArrayByClass<UPrimitiveComponent>();

		for (size_t ComponentIndex = 0; ComponentIndex < PrimitiveComponents.Size(); ++ComponentIndex)
		{
			UPrimitiveComponent* Primitive = PrimitiveComponents[ComponentIndex];
			if (!Primitive) continue;

			FMatrix ModelWorld = Primitive->GetComponentTransform();

			float T = RayIntersectsMesh(RayOrigin, RayDirection, Primitive->GetMesh(), ModelWorld);
			if (T > 0.f && T < MinT)
			{
				MinT = T;
				ClosestActor = TargetActor;
			}
		}
	}

	return ClosestActor;
}

// 공용 매시 충돌검사 함수 
float UWorld::RayIntersectsMesh(const FVector& RayOrigin, const FVector& RayDirection, const UMesh* Mesh, const FMatrix& WorldMatrix)
{
	if (!Mesh) return -1.f;

	FVector LocalRayOrigin = FMatrix::TransformCoord(RayOrigin, WorldMatrix.Inverse());
	FVector LocalRayDirection = FMatrix::TransformNormal(RayDirection, WorldMatrix.Inverse());

	const FVertexSimple* BufferData = static_cast<const FVertexSimple*>(Mesh->GetVertexData());

	float MinT = 1e9;
	for (uint64 i = 0; i < Mesh->GetVertexCount(); i += 3)
	{
		FVector V0 = { BufferData[i].X, BufferData[i].Y, BufferData[i].Z };
		FVector V1 = { BufferData[i + 1].X, BufferData[i + 1].Y, BufferData[i + 1].Z };
		FVector V2 = { BufferData[i + 2].X, BufferData[i + 2].Y, BufferData[i + 2].Z };

		float T;
		if (Math::RayIntersectsTriangle(LocalRayOrigin, LocalRayDirection, V0, V1, V2, T))
		{
			MinT = min(MinT, T);
		}
	}

	return (MinT == 1e9) ? -1.f : MinT;
}

bool UWorld::RayIntersectsSphere(const FVector& RayOrigin, const FVector& RayDir, const USceneComponent* SceneComponent, const FMatrix& WorldMatrix)
{
	if (!SceneComponent) return false;

	//구와 직선의 충돌 방정식을 구한다.
	//(Center - 레이의 한점) ^ 2 =  Radius ^ 2 ;
	//여기선 근의 공식을 사용하여 충돌 여부를 확인한다.

	FVector CenterToOrigin = (RayOrigin - SceneComponent->GetComponentLocation());
	FVector ObjectScale = SceneComponent->GetScale();
	float Radius = max(max(ObjectScale.X, ObjectScale.Y), ObjectScale.Z) * 3.f;

	float a = RayDir.Dot(RayDir);
	float b = 2 * CenterToOrigin.Dot(RayDir);
	float c = CenterToOrigin.Dot(CenterToOrigin) - Radius * Radius;

	return (b * b - 4 * a * c >= 0);
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
		if (params.bOverrideUUID)
			actor->UUID = params.UUID;

		if (params.PrimitiveType == "Cube")
		{
			UCubeComponent* Cube = actor->AddComponent<UCubeComponent>();
			Cube->SetMesh(resourceManager->FindMeshData(params.PrimitiveType));
			actor->RootComponent = Cube;
		}
		else if (params.PrimitiveType == "Sphere")
		{
			USphereComponent* Sphere = actor->AddComponent<USphereComponent>();
			Sphere->SetMesh(resourceManager->FindMeshData(params.PrimitiveType));
			actor->RootComponent = Sphere;
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

void UWorld::ClearScene()
{
	for (size_t i = 0; i < CurrentLevel->Actors.Size(); ++i)
	{
		UObjectFactory::DestroyObject(CurrentLevel->Actors[i]);
	}

	CurrentLevel->Actors.Clear();
}

