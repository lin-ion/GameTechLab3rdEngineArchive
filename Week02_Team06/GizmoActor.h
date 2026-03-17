#pragma once
#include "Actor.h"

enum class EGizmoAxis { Center, X, Y, Z, None };

class UArrowComponent;
class USphereComponent;

class UGizmoActor : public AActor
{
	DECLARE_CLASS(UGizmoActor, AActor)

public:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Release() override;

	EGizmoAxis CheckGizmoPicking();

public:
	UArrowComponent* ArrowY = nullptr; // Y축 (기본, Green)
	UArrowComponent* ArrowX = nullptr; // X축 (Red, Z축 -90도 회전)
	UArrowComponent* ArrowZ = nullptr; // Z축 (Blue, X축 +90도 회전)
	USphereComponent* BasePoint = nullptr;

	//추척된 참조

};
