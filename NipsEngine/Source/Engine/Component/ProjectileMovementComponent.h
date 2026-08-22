#pragma once
#include "MovementComponent.h"
/*
* Update Component를 주어진 Velocity로 Speed만큼 보낸다.
*/
class UProjectileMovementComponent : public UMovementComponent
{
public:
	DECLARE_CLASS(UProjectileMovementComponent, UMovementComponent)

	virtual UProjectileMovementComponent* Duplicate() override;
	// 발사체의 속도를 갱신해주는 함수.
	void BeginPlay() override;
	// 매 프레임 발사체를 시뮬레이션 하는 함수.
	void TickComponent(float DeltaTime) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	// 로컬 기준 속도값을 월드로 변환하기
	void SetVelocityInLocalSpace(const FVector& NewVelocity);
	// 가속도 계산 함수
	FVector ComputeAcceleration(const FVector& InVelocity, float DeltaTime) const;
	// 속도 계산 함수
	FVector ComputeVelocity(const FVector& InitialVelocity, float DeltaTime) const;
	// 계산된 속도값을 기반으로 새로운 속도를 얻어, DeltaTime만큼 거리 변화량을 반환.
	FVector ComputeMoveDelta(const FVector& InVelocity, float DeltaTime) const;
	float GetMaxSpeed() const { return MaxSpeed; }
	// 기본 중력상수에 가중치를 곱한 값을 반환하는 함수
	float GetGravityZ() const;

	void AddForce(const FVector& Force) { PendingForce += Force; }
	void ClearPendingForce() { PendingForce = FVector::ZeroVector; }

protected:
	// 속도 벡터를 기반으로 Rotation을 만드는 함수
	static FQuat MakeRotationFromVelocity(const FVector& InVelocity);

	// 발사체 초기 속도
	float InitialSpeed = 0.0f;
	// 발사체가 최대로 가질 수 있는 속도
	float MaxSpeed = 0.0f;
	// 중력을 얼마나 받을지 결정
	float ProjectileGravityScale = 1.0f;
	// 발사체가 이동방향을 바라보게 할지 여부
	bool bRotationFollowsVelocity = false;
	// 초기 velocity를 world기준으로 볼지, local 기준으로 볼지 여부
	bool bInitialVelocityInLocalSpace = true;
	// 이번 프레임 말고, 다음 시뮬레이션 업데이트 때 속도에 반영할 힘을 모아두는 변수
	
	// TODO: 폭발, 바람, 유도력 같은 추가적인 기능대비 잔존
	FVector PendingForce = FVector::ZeroVector;
};

