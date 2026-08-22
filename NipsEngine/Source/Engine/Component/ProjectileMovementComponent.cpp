#include "ProjectileMovementComponent.h"
#include "Component/SceneComponent.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Math/Utils.h"
#include "Object/ObjectFactory.h"

#include <cmath>

DEFINE_CLASS(UProjectileMovementComponent, UMovementComponent)
REGISTER_FACTORY(UProjectileMovementComponent)

namespace
{
	constexpr float DefaultGravityZ = -9.8f; // 9.8 m/(s^2) 중력 상수
}

UProjectileMovementComponent* UProjectileMovementComponent::Duplicate()
{
	UProjectileMovementComponent* NewComp = UObjectManager::Get().CreateObject<UProjectileMovementComponent>();
	// 공통 Shallow Copy는 부모클래스에 있는 함수 사용
	CopyMovementStateTo(NewComp);
	NewComp->bCreatedInEditorInstance = bCreatedInEditorInstance;

	// ProjectileComponent만의 특성을 여기에 기입
	NewComp->InitialSpeed = InitialSpeed;
	NewComp->MaxSpeed = MaxSpeed;
	NewComp->ProjectileGravityScale = ProjectileGravityScale;
	NewComp->bRotationFollowsVelocity = bRotationFollowsVelocity;
	NewComp->bInitialVelocityInLocalSpace = bInitialVelocityInLocalSpace;
	NewComp->PendingForce = FVector::ZeroVector;
	NewComp->DuplicateSubObjects();
	return NewComp;
}

void UProjectileMovementComponent::BeginPlay()
{
	UMovementComponent::BeginPlay();

	if (Velocity.IsNearlyZero())
	{
		return;
	}

	if (InitialSpeed > 0.0f)
	{
		// Velocity 방향만 사용
		Velocity = Velocity.GetSafeNormal() * InitialSpeed;
	}

	if (bInitialVelocityInLocalSpace)
	{
		// 초기값으로 받은 Local Space값을 World Space로 변환해주는 함수.
		SetVelocityInLocalSpace(Velocity);
	}

	if (bRotationFollowsVelocity && UpdatedComponent != nullptr)
	{
		// 발사체가 이동방향을 바라보는 옵션이 켜져있다면, 발사체의 rotation만 Velocity방향으로 회전시켜준다.
		const FQuat NewRotation = MakeRotationFromVelocity(Velocity);
		MoveUpdatedComponent(FVector::ZeroVector, NewRotation);
	}
	// 위의 식을 거치면서 Velocity가 변화됐을 수 있으니 갱신.
	UpdateComponentVelocity();
}

void UProjectileMovementComponent::TickComponent(float DeltaTime)
{
	// 업데이트 여부 검사
	if (ShouldSkipUpdate(DeltaTime))
	{
		return;
	}

	UMovementComponent::TickComponent(DeltaTime);

	if (UpdatedComponent == nullptr)
	{
		return;
	}

	// 구 속도값과 deltatime만큼 변한 위치값을 받아온다.
	const FVector OldVelocity = Velocity;
	// 이번 deltaTime 이동량을 계산한다.
	const FVector MoveDelta = ComputeMoveDelta(OldVelocity, DeltaTime);
	// component의 World 회전값을 가져온다
	FQuat NewRotation = UpdatedComponent->GetWorldTransform().GetRotation();
	// rotation이 속도 벡터를 따라가야하고, 이전 속도가 0이 아니라면 newrotation을 vector 방향으로 갱신
	if (bRotationFollowsVelocity && !OldVelocity.IsNearlyZero())
	{
		NewRotation = MakeRotationFromVelocity(OldVelocity);
	}
	// 새로운 rotation값과 delta값 기반으로로 component 위치 갱신
	MoveUpdatedComponent(MoveDelta, NewRotation);
	// 새 속도 계산 및 갱신
	Velocity = ComputeVelocity(OldVelocity, DeltaTime);
	UpdateComponentVelocity();
	// 지금까지 누적된 힘 초기화
	ClearPendingForce();
}

void UProjectileMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UMovementComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Initial Speed", EPropertyType::Float, &InitialSpeed, 0.0f, 100000.0f, 1.0f });
	OutProps.push_back({ "Max Speed", EPropertyType::Float, &MaxSpeed, 0.0f, 100000.0f, 1.0f });
	OutProps.push_back({ "Gravity Scale", EPropertyType::Float, &ProjectileGravityScale, -10.0f, 10.0f, 0.1f });
	OutProps.push_back({ "Rotation Follows Velocity", EPropertyType::Bool, &bRotationFollowsVelocity });
	OutProps.push_back({ "Initial Velocity In Local Space", EPropertyType::Bool, &bInitialVelocityInLocalSpace });
}

void UProjectileMovementComponent::SetVelocityInLocalSpace(const FVector& NewVelocity)
{
	if (UpdatedComponent != nullptr)
	{
		// UpdatedComponent의 World Transform값을 가져와서 
		// NewVelocity값의 회전 벡터만 적용해서 Local->World로 만든다.
		Velocity = UpdatedComponent->GetWorldTransform().TransformVectorNoScale(NewVelocity);
	}
	else
	{
		Velocity = NewVelocity;
	}
}

FVector UProjectileMovementComponent::ComputeAcceleration(const FVector& InVelocity, float DeltaTime) const
{
	// Pending된 힘을 더한다.
	FVector Acceleration = PendingForce;
	// 중력가속도를 더한다.
	Acceleration.Z += GetGravityZ();
	return Acceleration;
}

FVector UProjectileMovementComponent::ComputeVelocity(const FVector& InitialVelocity, float DeltaTime) const
{
	// 새 속도 = 초기 속도 (v0) + 가속도 * 시간 -> v = v0 + a * t
	FVector NewVelocity = InitialVelocity + ComputeAcceleration(InitialVelocity, DeltaTime) * DeltaTime;
	// 속도 clamp
	const float CurrentMaxSpeed = GetMaxSpeed();
	if (CurrentMaxSpeed > 0.0f)
	{
		const float SpeedSq = NewVelocity.SizeSquared();
		const float MaxSpeedSq = CurrentMaxSpeed * CurrentMaxSpeed;
		if (SpeedSq > MaxSpeedSq)
		{
			NewVelocity = NewVelocity.GetSafeNormal() * CurrentMaxSpeed;
		}
	}
	return NewVelocity;
}

FVector UProjectileMovementComponent::ComputeMoveDelta(const FVector& InVelocity, float DeltaTime) const
{
	const FVector NewVelocity = ComputeVelocity(InVelocity, DeltaTime);
	// 등가속도 운동 위치 변화량 공식
	// S = v0 * t + a * t *  1/2 * t
	return (InVelocity * DeltaTime) + (NewVelocity - InVelocity) * (0.5f * DeltaTime);
}

float UProjectileMovementComponent::GetGravityZ() const
{
	return DefaultGravityZ * ProjectileGravityScale;
}

FQuat UProjectileMovementComponent::MakeRotationFromVelocity(const FVector& InVelocity)
{
	// 입력으로 받은 velocity값을 normalize해 방향을 알아낸다
	const FVector Direction = InVelocity.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return FQuat::Identity;
	}

	// 구면좌표계를 사용해서 Yaw, Pitch 계산

	// 속도벡터에서 Yaw 추출
	// XY 평면에서의 방향각도 추출 (X, Y)가 X축과 이루는 각도: atan2(Y/X))
	const float YawDegrees = MathUtil::RadiansToDegrees(std::atan2(Direction.Y, Direction.X));

	// 방향벡터를 XY 평면에 눌렀을 때 길이(내적, 바닥면에서 얼마나 뻗어있는지 구하기)
	const float FlatLength = std::sqrt(Direction.X * Direction.X + Direction.Y * Direction.Y);

	// 속도벡터에서 Pitch 추출
	// 수평면 대비 위/아래 각도(Direction.Z: 얼마나 위아래인지, FlatLength: 수평으로 얼마나 가는지)
	float PitchDegrees = MathUtil::RadiansToDegrees(std::atan2(Direction.Z, FlatLength));

	return FRotator(PitchDegrees, YawDegrees, 0.0f).Quaternion();
}
