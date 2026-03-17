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

public:
	UArrowComponent* ArrowY = nullptr; // Y축 초
	UArrowComponent* ArrowX = nullptr; // X축 빨
	UArrowComponent* ArrowZ = nullptr; // Z축 파
	USphereComponent* BasePoint = nullptr;

	//추척된 참조

};
