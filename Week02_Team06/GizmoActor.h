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

	// 기즈모 컴포넌트 색상 설정
private:
	const FVector4 ColorX = { 1.f, 0.f, 0.f, 1.f };      // 빨강
	const FVector4 ColorY = { 0.f, 1.f, 0.f, 1.f };      // 초록
	const FVector4 ColorZ = { 0.f, 0.f, 1.f, 1.f };      // 파랑
	const FVector4 ColorCenter = { 1.f, 1.f, 1.f, 1.f }; // 흰색
	const FVector4 ColorHover = { 1.f, 0.5f, 0.f, 1.f }; // 주황
};
