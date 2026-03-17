#pragma once
#include "Object.h"
#include "Level.h"
#include "ImGuiDrawer.h"
#include "Math.h"

class UMesh;
class AActor;
class UGizmoComponent;
class UResourceManager;
class FEditorViewportClient;
class UPrimitiveComponent;

class UWorld : public UObject
{
public:
	UWorld() = default;
	virtual ~UWorld() = default;

public:
	/** Actor를 생성하고 PersistentLevel에 등록한 뒤 BeginPlay 호출 */
	template<typename T>
	T* SpawnActor()
	{
		static_assert(std::is_base_of_v<AActor, T>, "T must derive from AActor");

		//언리얼 5로 가면서 NewObject로 바뀌었다고는 하는데.. 교차검증 필요 
		//ConstructObject와 뭐가 다른건지
		T* Actor           = NewObject<T>();

		Actor->OwningLevel = CurrentLevel;

		if (CurrentLevel)
		{
			(CurrentLevel->Actors).PushBack(Actor);
		}

		Actor->BeginPlay();
		return Actor;
	}
	virtual void Release() override;

public:
	virtual void InitWorld(UResourceManager& ResourceManager, FEditorViewportClient* _ViewPort);
	void Tick(float DeltaTime);
	
	AActor* GetPickedActor();
	void SpawnActorFromEditor(FSpawnParameters params);
	
	bool RayIntersectsMesh(const FVector& RayOrigin, const FVector& RayDir, const UMesh* Mesh, const FMatrix& WorldMatrix); // 공용 매시 충돌검사 함수

private:
	class UGizmoActor* MainGizmoActor = { nullptr };

private:
	bool RayIntersectsTriangle(const FVector& CameraPos, const FVector& CameraRay, const FVertexSimple& V0, const FVertexSimple& V1, const FVertexSimple& V2);
	//void PickActor(const FVector& RayOrigin, const FVector& RayDir);
	AActor* GetSelectedActor() const { return SelectedActor; }
	FVector CalculateClosestPointOnAxis(const FVector& RayOrigin, const FVector& RayDir, EGizmoAxis Axis);
	// [추가] 마우스 광선과 가상 평면이 만나는 3D 교차점을 반환하는 수학 도우미
	FVector CalculateRayPlaneIntersection(const FVector& RayOrg, const FVector& RayDir, const FVector& PlaneNormal, const FVector& PlaneOrigin);


public:
	// 월드가 시작할 때 초기 레벨
	//원래는 월드가 바뀌어도 그려지는 PersisteneLevel과 StreamingLevel로 구별됨
	ULevel* CurrentLevel = { nullptr };
	FEditorViewportClient* ViewPort = { nullptr };

	//Picked된 Actor
	AActor* PickedActor = { nullptr };


	UResourceManager* resourceManager;

	//나중엔 씬을 만들것임
	//FScene* Scene = { nullptr };
private:
	AActor* SelectedActor = { nullptr };

	// 드래그 관련 상태 변수
	EGizmoAxis CurrentDraggingAxis = EGizmoAxis::None; // 현재 잡고 있는 축
	FVector DragStartPoint = { 0.f, 0.f, 0.f };      // 클릭한 시점의 3D 축 위 좌표
	FVector GizmoStartLocation = { 0.f, 0.f, 0.f };  // 클릭한 시점의 기즈모 위치
	bool bIsDragging = false;    // 드래그 중인지 여부

	//void TransferGizmo(AActor* NewTarget);
};

