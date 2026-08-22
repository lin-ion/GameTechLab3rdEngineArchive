#include "RotatingMovementComponent.h"
#include "Component/SceneComponent.h"
#include "Math/Quat.h"
#include "Math/Utils.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(URotatingMovementComponent, UMovementComponent)
REGISTER_FACTORY(URotatingMovementComponent)

URotatingMovementComponent* URotatingMovementComponent::Duplicate()
{
	URotatingMovementComponent* NewComp = UObjectManager::Get().CreateObject<URotatingMovementComponent>();
	NewComp->bCreatedInEditorInstance = bCreatedInEditorInstance;

	CopyMovementStateTo(NewComp);
	NewComp->RotationRate = RotationRate;
	NewComp->PivotTranslation = PivotTranslation;
	NewComp->bRotationInLocalSpace = bRotationInLocalSpace;
	NewComp->DuplicateSubObjects();
	return NewComp;
}

void URotatingMovementComponent::TickComponent(float DeltaTime)
{
	if (ShouldSkipUpdate(DeltaTime))
	{
		return;
	}

	if (UpdatedComponent == nullptr)
	{
		return;
	}

	// Compute New Rotation
	
	// 이전 Component의 위치
	const FQuat OldRotation = UpdatedComponent->GetWorldTransform().GetRotation();
	// DeltaTime 동안 회전량 계산
	const FQuat DeltaQuat = FQuat::MakeFromEuler(RotationRate * DeltaTime);
	// Local Space 여부에 따라 곱셈 순서 부여 (Unreal Engine하고 반대인 이유: 쿼터니언 * 연산자 정의가 달라서)
	const FQuat NewRotation = bRotationInLocalSpace ? (DeltaQuat * OldRotation) : (OldRotation * DeltaQuat);

	// Compute New Location
	FVector DeltaLocation = FVector::ZeroVector;
	if (!PivotTranslation.IsNearlyZero())
	{
		// Quaternion으로 벡터를 회전시켰을 때 pivot 차이만큼 Update 한다
		const FVector OldPivot = OldRotation.RotateVector(PivotTranslation); // 회전 전 pivot 끝점 위치
		const FVector NewPivot = NewRotation.RotateVector(PivotTranslation); // 회전 후 pivot 끝점 위치
		DeltaLocation = OldPivot - NewPivot; // 그 차이만큼 원본을 이동시킨다. 즉, pivot의 끝이 중점이 되는거다.
	}

	MoveUpdatedComponent(DeltaLocation, NewRotation); // 실제로 원본을 이동시키는 코드
	Velocity = DeltaTime > MathUtil::Epsilon ? (DeltaLocation / DeltaTime) : FVector::ZeroVector; //속도는 거리/시간
	UpdateComponentVelocity();
}

void URotatingMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	// UMovementComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Rotation Rate", EPropertyType::Vec3, &RotationRate, 0.0f, 0.0f, 0.1f });
	OutProps.push_back({ "Pivot Translation", EPropertyType::Vec3, &PivotTranslation, 0.0f, 0.0f, 0.1f });
	OutProps.push_back({ "Local Space", EPropertyType::Bool, &bRotationInLocalSpace });
}
