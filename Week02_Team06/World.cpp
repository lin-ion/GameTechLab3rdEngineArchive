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
#include "Mesh.h"


void UWorld::InitWorld(UResourceManager& ResourceManager, FEditorViewportClient* _ViewPort)
{
	CurrentLevel = UObjectFactory::NewObject<ULevel>();
    AActor* GizmoStorageActor = SpawnActor<AActor>();

	CurrentLevel->OwningWorld = this;
	ViewPort = _ViewPort;


	resourceManager = &ResourceManager;

	AActor* CubeActor = SpawnActor<AActor>();
	MainGizmoActor = SpawnActor<UGizmoActor>();


	//레벨에 엑터 추가
	UCubeComponent* CubeComponent = CubeActor->AddComponent<UCubeComponent>();
	CubeComponent->SetMesh(ResourceManager.FindMeshData("Cube"));
	CubeComponent->SetHovering(true);
	CubeActor->RootComponent = CubeComponent;


	MainGizmoActor->ArrowY->SetMesh(ResourceManager.FindMeshData("Gizmo"));
	MainGizmoActor->ArrowX->SetMesh(ResourceManager.FindMeshData("Gizmo"));
	MainGizmoActor->ArrowZ->SetMesh(ResourceManager.FindMeshData("Gizmo"));
	MainGizmoActor->BasePoint->SetMesh(ResourceManager.FindMeshData("Sphere"));

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

	// 드래그 중이라면
	UInput& Input = UInput::GetInstance();
	FVector RayOrigin = ViewPort->GetViewLocation();
	FVector RayDir = ViewPort->GetCameraRayDirection();

	// 마우스 좌클릭 시 피킹
	if (Input.IsKeyDown(VK_LBUTTON))
	{
		if (MainGizmoActor)
		{
			// 현재 마우스가 기즈모의 어느 축 위에 있는지 확인
			CurrentDraggingAxis = MainGizmoActor->CheckGizmoPicking();

			if (CurrentDraggingAxis != EGizmoAxis::None)
			{
				bIsDragging = true;
				GizmoStartLocation = MainGizmoActor->RootComponent->GetPosition();

				// Center를 잡았을 때는 평면 투영, 그 외에는 선 투영
				if (CurrentDraggingAxis == EGizmoAxis::Center)
				{
					FVector PlaneNormal = RayOrigin - GizmoStartLocation;
					PlaneNormal.Normalize();
					DragStartPoint = Math::RayPlaneIntersection(RayOrigin, RayDir, PlaneNormal, GizmoStartLocation);
				}
				else
				{
					DragStartPoint = CalculateClosestPointOnAxis(RayOrigin, RayDir, CurrentDraggingAxis);
				}
			}
		}
		
		else
		{
			// 기즈모가 아니라면 월드 공간의 액터를 피킹합니다.
			AActor* HitActor = GetPickedActor();

			// 액터가 선택되었다면 기즈모를 해당 액터로 이사시킵니다.
			if (HitActor)
			{
				//TransferGizmo(HitActor);   
			}
		}
	}

	// 버튼을 누르고 있는 상태 (IsKeyPressing)
	if (bIsDragging && Input.IsKeyPressing(VK_LBUTTON))
	{
		FVector CurrentPoint;

		// 이동 중에도 동일한 방식으로 투영점 계산
		if (CurrentDraggingAxis == EGizmoAxis::Center)
		{
			FVector PlaneNormal = RayOrigin - GizmoStartLocation;
			PlaneNormal.Normalize();
			CurrentPoint = Math::RayPlaneIntersection(RayOrigin, RayDir, PlaneNormal, GizmoStartLocation);
		}
		else
		{
			CurrentPoint = CalculateClosestPointOnAxis(RayOrigin, RayDir, CurrentDraggingAxis);
		}

		// 이동 변위(Delta) = 현재 지점 - 시작 지점
		FVector Delta = CurrentPoint - DragStartPoint;

		// 기즈모를 시작 위치에서 변위만큼 이동
		MainGizmoActor->RootComponent->SetPosition(GizmoStartLocation + Delta);
	}

	// 3. 버튼을 뗀 경우 (IsKeyUp)
	if (Input.IsKeyUp(VK_LBUTTON))
	{
		bIsDragging = false;
		CurrentDraggingAxis = EGizmoAxis::None;
	}
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

// 가장 가까운 것만 골라가도록 추후 수정
AActor* UWorld::GetPickedActor()
{
	auto ActorArray = CurrentLevel->Actors;

	for (size_t ActorIndex = 0; ActorIndex < ActorArray.Size(); ++ActorIndex)
	{
		AActor* TargetActor = ActorArray[ActorIndex];
		// [핵심 방어] 기즈모를 담고 있는 액터 자체는 피킹 검수에서 제외합니다.
		if (MainGizmoActor == TargetActor) continue;

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
				FVector V0 = { BufferData[vertexIndex].X,     BufferData[vertexIndex].Y,     BufferData[vertexIndex].Z };
				FVector V1 = { BufferData[vertexIndex + 1].X, BufferData[vertexIndex + 1].Y, BufferData[vertexIndex + 1].Z };
				FVector V2 = { BufferData[vertexIndex + 2].X, BufferData[vertexIndex + 2].Y, BufferData[vertexIndex + 2].Z };

				if (Math::RayIntersectsTriangle(_CameraPos, _CameraRay, V0, V1, V2))
				{
					PickedActor = TargetActor;
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

FVector UWorld::CalculateClosestPointOnAxis(const FVector& RayOrg, const FVector& RayDir, EGizmoAxis Axis)
{
	if (!MainGizmoActor) return FVector::Zero;

	FVector AxisDir;
	switch (Axis)
	{
	case EGizmoAxis::X: AxisDir = MainGizmoActor->ArrowX->GetUpVector(); break;
	case EGizmoAxis::Y: AxisDir = MainGizmoActor->ArrowY->GetUpVector(); break;
	case EGizmoAxis::Z: AxisDir = MainGizmoActor->ArrowZ->GetUpVector(); break;
	default: return MainGizmoActor->RootComponent->GetPosition();
	}

	FVector GizmoPos = MainGizmoActor->RootComponent->GetPosition();

	// [수정] 복잡한 공식은 지우고 Math 모듈 호출
	return Math::ClosestPointOnLine(RayOrg, RayDir, GizmoPos, AxisDir);
}

void UWorld::Release()
{
	// World는 참조만 정리
	CurrentLevel = nullptr;
}