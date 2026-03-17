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
					PlaneNormal.Normalize(); // 카메라를 바라보는 수직 벡터
					DragStartPoint = CalculateRayPlaneIntersection(RayOrigin, RayDir, PlaneNormal, GizmoStartLocation);
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
			CurrentPoint = CalculateRayPlaneIntersection(RayOrigin, RayDir, PlaneNormal, GizmoStartLocation);
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

	// 두 직선(마우스 광선, 기즈모 축) 간의 최단 거리 계수 t를 구하는 공식
	FVector w0 = RayOrg - GizmoPos;
	float a = RayDir.Dot(RayDir);
	float b = RayDir.Dot(AxisDir);
	float c = AxisDir.Dot(AxisDir);
	float d = RayDir.Dot(w0);
	float e = AxisDir.Dot(w0);

	float Denom = a * c - b * b;
	if (abs(Denom) < 1e-6f) return GizmoPos; // 평행할 경우

	// t2는 기즈모 축 직선상의 매개변수
	float t2 = (a * e - b * d) / Denom;

	// 최종 월드 좌표 반환
	return GizmoPos + (AxisDir * t2);
}

FVector UWorld::CalculateRayPlaneIntersection(const FVector& RayOrg, const FVector& RayDir, const FVector& PlaneNormal, const FVector& PlaneOrigin)
{
	// 광선과 평면 법선의 내적 계산 (0이면 광선과 평면이 서로 평행함)
	float Denom = RayDir.Dot(PlaneNormal);
	if (abs(Denom) < 1e-6f) return PlaneOrigin;

	// 교차점 계수 t = ((평면원점 - 광선원점) 내적 법선) / Denom
	FVector Diff = PlaneOrigin - RayOrg;
	float t = Diff.Dot(PlaneNormal) / Denom;

	// 광선의 원점에서 방향(RayDir)으로 t만큼 이동한 최종 교차점 반환
	return RayOrg + (RayDir * t);
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

// 공용 매시 충돌검사 함수
bool UWorld::RayIntersectsMesh(const FVector& RayOrigin, const FVector& RayDir, const UMesh* Mesh, const FMatrix& WorldMatrix)
{
	if (!Mesh) return false;

	// 광선을 월드 공간에서 로컬 공간으로 변환
	FVector _CameraPos = FMatrix::TransformCoord(RayOrigin, WorldMatrix.Inverse());
	FVector _CameraRay = FMatrix::TransformNormal(RayDir, WorldMatrix.Inverse());

	const FVertexSimple* BufferData = static_cast<const FVertexSimple*>(Mesh->GetVertexData());

	for (uint64 vertexIndex = 0; vertexIndex < Mesh->GetVertexCount(); vertexIndex += 3)
	{
		if (RayIntersectsTriangle(_CameraPos, _CameraRay, BufferData[vertexIndex], BufferData[vertexIndex + 1], BufferData[vertexIndex + 2]))
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