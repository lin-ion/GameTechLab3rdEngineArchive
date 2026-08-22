#pragma once
#include "ActorComponent.h"

class USceneComponent;
/*
* Actor의 RootComponent를 참조해서 이동 관련 처리를해주는 컴포넌트
* 무엇을 움직일지를 관리한다
* Movement, Rotating, Projectile 함수의 경우 언리얼엔진의 소스코드를 많이 참고하여 제작
*/
class UMovementComponent : public UActorComponent
{
public:
	DECLARE_CLASS(UMovementComponent, UActorComponent)

	// PIE를 위한 Duplicate 함수
	virtual UMovementComponent* Duplicate() override;
	virtual UMovementComponent* DuplicateSubObjects() override { return this; }

	// Component Begin Play (UpdatedComponent가 없을 경우 Owner한테서 RootComponent를 가져온다)
	virtual void BeginPlay() override;
	// 자식 클래스의 운동 공식이 여기에 들어간다.
	virtual void TickComponent(float DeltaTime) override;
	// ImGui에서 띄울 UI 목록
	virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	// 이동시킬 컴포넌트를 연결하는 함수(꼭 RootComponent가 아니어도 상관없다)
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent); 

	// TickComponent로 계산된 이동값을 실제로 SceneComponent에 전달되는 함수
	virtual bool MoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation);
	// UpdateComponent가 없거나, DeltaTime이 epsilon 미만이라면 Upate를 진행하지 않는다.
	virtual bool ShouldSkipUpdate(float DeltaTime) const;

	// 즉시 UpdateComponent의 움직임을 중단시키는 함수(Vector를 0으로 만든다)
	virtual void StopMovementImmediately();
	// 해당 컴포넌트에서 캐싱하는 컴포넌트의 속도를 갱신하는 함수
	virtual void UpdateComponentVelocity();

	// Velocity 및 UpdatedComponent GetSet 함수
	void SetVelocity(const FVector& NewVelocity) { Velocity = NewVelocity; }
	const FVector& GetVelocity() const { return Velocity; }
	USceneComponent* GetUpdatedComponent() const { return UpdatedComponent; }

protected:
	// Duplicate Helper Function (Movement 상태값을 복사)
	void CopyMovementStateTo(UMovementComponent* NewComp) const;

	USceneComponent* UpdatedComponent = nullptr;
	FVector Velocity = { 0.f, 0.f, 0.f };
};

