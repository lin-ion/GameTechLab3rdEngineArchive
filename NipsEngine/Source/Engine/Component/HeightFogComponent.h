#pragma once
#pragma once

#include "PrimitiveComponent.h"
#include "Render/Common/RenderTypes.h"  // FColor


/**
 * UHeightFogComponent
 *
 * Exponential Height Fog + Basic Inscattering 구현체.
 * 씬에 하나만 존재하는 전역 안개 효과를 기술하는 컴포넌트입니다.
 *
 * 안개 농도 공식 (UE4 기준):
 *   FogFactor = FogDensity * exp( -FogHeightFalloff * max(WorldPos.Z - ComponentZ, 0) )
 *   Extinction = 1 - exp( -FogFactor * ViewDistance )
 *   FinalColor  = lerp(SceneColor, InscatteringColor, min(Extinction, FogMaxOpacity))
 *
 * StartDistance / FogCutoffDistance 로 안개가 시작/끝나는 거리를 제한합니다.
 */
class UHeightFogComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UHeightFogComponent, UPrimitiveComponent)

	UHeightFogComponent();
	virtual ~UHeightFogComponent();
	// ---------------------------------------------------------------
	//  UPrimitiveComponent 순수 가상 함수 구현
	// ---------------------------------------------------------------
	UHeightFogComponent* Duplicate()           override;
	UHeightFogComponent* DuplicateSubObjects()  override { return this; }

	void  UpdateWorldAABB()  const override;
	bool  RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;

	EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_HeightFog; }

	/** HeightFog는 오브젝트 선택 아웃라인을 그리지 않습니다. */
	bool SupportsOutline() const override { return true; }

	// ---------------------------------------------------------------
	//  Property window 연동
	// ---------------------------------------------------------------
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	// ---------------------------------------------------------------
	//  Fog 파라미터 Getter / Setter
	// ---------------------------------------------------------------

	/** 안개의 전체 밀도 (≥ 0). 클수록 짙어집니다. */
	float GetFogDensity()        const { return FogDensity; }
	void  SetFogDensity(float V);

	/**
	 * 높이에 따른 밀도 감쇠율 (≥ 0).
	 * 0 이면 고도 변화 없이 균일한 안개.
	 * 클수록 아래쪽에만 안개가 집중됩니다.
	 */
	float GetFogHeightFalloff()  const { return FogHeightFalloff; }
	void  SetFogHeightFalloff(float V);

	/** 카메라로부터 안개가 시작되는 최소 거리 (월드 유닛, ≥ 0). */
	float GetStartDistance()     const { return StartDistance; }
	void  SetStartDistance(float V);

	/**
	 * 안개가 완전히 사라지는 최대 거리 (월드 유닛).
	 * 0 이하면 무제한으로 처리합니다.
	 */
	float GetFogCutoffDistance() const { return FogCutoffDistance; }
	void  SetFogCutoffDistance(float V);

	/** 안개의 최대 불투명도 [0, 1]. 1 이면 완전히 가려집니다. */
	float GetFogMaxOpacity()     const { return FogMaxOpacity; }
	void  SetFogMaxOpacity(float V);

	/** 안개의 산란(Inscattering) 색상. */
	FColor GetFogInscatteringColor() const { return FogInscatteringColor; }
	void   SetFogInscatteringColor(const FColor& Color) { FogInscatteringColor = Color; }

	// ---------------------------------------------------------------
	//  CPU-side 안개 계산 유틸리티
	//  (렌더러가 GPU 셰이더를 사용하지 않는 경우 또는 에디터 프리뷰용)
	// ---------------------------------------------------------------

	/**
	 * 주어진 월드 위치와 카메라 위치로 안개 인수(0~FogMaxOpacity)를 반환합니다.
	 *
	 * @param WorldPos       샘플링할 월드 공간 위치
	 * @param CameraPos      카메라(시점) 위치
	 * @return               안개 혼합 비율 [0, FogMaxOpacity]
	 */
	float ComputeFogFactor(const FVector& WorldPos, const FVector& CameraPos) const;

	/**
	 * SceneColor 에 안개를 블렌딩한 최종 색상을 반환합니다. (CPU 프리뷰 전용)
	 *
	 * @param SceneColor     원본 픽셀 색상
	 * @param WorldPos       해당 픽셀의 월드 위치
	 * @param CameraPos      카메라 위치
	 */
	FColor ApplyFog(const FColor& SceneColor, const FVector& WorldPos, const FVector& CameraPos) const;

	bool GetFogExist() { return bIsFog; }
	void SetFogExist(bool InbIsFog) { bIsFog = InbIsFog; }

private:
	// ---------------------------------------------------------------
	//  Fog 파라미터
	// ---------------------------------------------------------------

	/** 전체 밀도 스케일. UE4 기본값: 0.02 */
	float FogDensity = 0.005f;  // 0.02 → 0.0002 (셰이더 * 0.01 제거 시)

	/** 높이 감쇠 계수. UE4 기본값: 0.2 */
	float FogHeightFalloff = 0.5f;   // 높이 감쇠를 더 강하게 → 낮은 곳만 안개

	/** 안개 시작 거리 (월드 유닛). */
	float  StartDistance = 3.0f;

	/** 안개 종료 거리 (0 이하 = 무제한). */
	float  FogCutoffDistance = 0.0f;

	/** 최대 불투명도 [0, 1]. */
	float  FogMaxOpacity = 0.003f;

	/** Inscattering 색상. */
	FColor FogInscatteringColor = FColor(200.f, 220.f, 255.f, 255.f);  // 연한 하늘색

	bool bIsFog = { false };
};