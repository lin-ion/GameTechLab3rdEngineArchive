#pragma once
#include "Component/MovementComponent.h"

/*
* Update Component를 주어진 값에 따라 회전시킨다
*/

class URotatingMovementComponent : public UMovementComponent
{
public:
	DECLARE_CLASS(URotatingMovementComponent, UMovementComponent)

	URotatingMovementComponent* Duplicate() override;

	// RotationRate기반의 세부 구현
	void TickComponent(float DeltaTime) override;

	// ImGui로 Control할 Parameter 전달
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

protected:
	// 1s에 RotationRate만큼 회전한다.
	FVector RotationRate = { 0.0f, 0.0f, 180.0f };
	// 물체가 회전하는 기준점(회전 중심까지 Local Offset)
	FVector PivotTranslation = FVector::ZeroVector;
	bool bRotationInLocalSpace = true;
};

