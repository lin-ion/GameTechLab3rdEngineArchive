#include "HeightFogComponent.h"
#include "Math/Utils.h"          // MathUtil::Epsilon
#include <cmath>
#include <algorithm>

DEFINE_CLASS(UHeightFogComponent, UPrimitiveComponent)
REGISTER_FACTORY(UHeightFogComponent);

// -----------------------------------------------------------------------
//  생성자
// -----------------------------------------------------------------------
UHeightFogComponent::UHeightFogComponent()
{
	// HeightFog 는 씬 전체에 적용되는 볼륨이므로
	// 기본적으로 레이캐스트 선택 대상에서 제외하지 않습니다.
	// 필요하다면 bIsVisible = false 로 초기화해도 됩니다.
}

UHeightFogComponent::~UHeightFogComponent()
{
	bIsFog = false;
}

// -----------------------------------------------------------------------
//  Duplicate
// -----------------------------------------------------------------------
UHeightFogComponent* UHeightFogComponent::Duplicate()
{
	UHeightFogComponent* NewComp = UObjectManager::Get().CreateObject<UHeightFogComponent>();

	NewComp->SetActive(this->IsActive());
	NewComp->SetOwner(nullptr);

	NewComp->SetRelativeLocation(this->GetRelativeLocation());
	NewComp->SetRelativeRotation(this->GetRelativeRotation());
	NewComp->SetRelativeScale(this->GetRelativeScale());

	NewComp->SetVisibility(this->IsVisible());

	// HeightFogComponent
	NewComp->FogDensity = FogDensity;
	NewComp->FogHeightFalloff = FogHeightFalloff;
	NewComp->StartDistance = StartDistance;
	NewComp->FogCutoffDistance = FogCutoffDistance;
	NewComp->FogMaxOpacity = FogMaxOpacity;
	NewComp->FogInscatteringColor = FogInscatteringColor;
	NewComp->bIsFog = bIsFog;

	//NewComp->OverrideMaterial = this->OverrideMaterial;
	//NewComp->ScrollUV = this->ScrollUV;

	//// 에셋 포인터는 얕은 복사로 동일한 원본 리소스를 참조하게 합니다.
	//NewComp->StaticMeshAsset = this->StaticMeshAsset;
	//NewComp->StaticMeshAssetPath = this->StaticMeshAssetPath;

	//// Dirty 플래그를 초기화해 복사 후 상태를 업데이트하도록 합니다.
	//NewComp->bBoundsDirty = true;
	//NewComp->bRenderStateDirty = true;

	NewComp->DuplicateSubObjects();

	return NewComp;
}

// -----------------------------------------------------------------------
//  AABB
//  HeightFog 는 씬 전체를 덮는 개념이므로 AABB 를 무한대에 가깝게 설정합니다.
//  실제로는 렌더러가 뷰 frustum 에 꽉 찬 쿼드를 그리므로 AABB 충돌보다는
//  컴포넌트의 존재 여부만 중요합니다.
// -----------------------------------------------------------------------
void UHeightFogComponent::UpdateWorldAABB() const
{
	constexpr float kHalfExtent = 1e6f;   // 사실상 무한 볼륨
	const FVector Center = GetWorldLocation();
	WorldAABB = FAABB(
		Center - FVector(kHalfExtent, kHalfExtent, kHalfExtent),
		Center + FVector(kHalfExtent, kHalfExtent, kHalfExtent)
	);
}

// -----------------------------------------------------------------------
//  RaycastMesh
//  HeightFog 는 메시를 갖지 않으므로 레이캐스트는 항상 실패합니다.
//  (선택하려면 에디터에서 별도 아이콘 스프라이트를 사용하세요.)
// -----------------------------------------------------------------------
bool UHeightFogComponent::RaycastMesh(const FRay& /*Ray*/, FHitResult& /*OutHitResult*/)
{
	return false;
}

// -----------------------------------------------------------------------
//  Property window
// -----------------------------------------------------------------------
void UHeightFogComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);

	OutProps.push_back({ "FogDensity",           EPropertyType::Float, &FogDensity });
	OutProps.push_back({ "FogHeightFalloff",      EPropertyType::Float, &FogHeightFalloff });
	OutProps.push_back({ "StartDistance",         EPropertyType::Float, &StartDistance });
	OutProps.push_back({ "FogCutoffDistance",     EPropertyType::Float, &FogCutoffDistance });
	OutProps.push_back({ "FogMaxOpacity",         EPropertyType::Float, &FogMaxOpacity });
	OutProps.push_back({ "FogInscatteringColor",  EPropertyType::Vec3, &FogInscatteringColor.r});
	OutProps.push_back({ "On/OFF",  EPropertyType::Bool, &bIsFog });
}

void UHeightFogComponent::PostEditProperty(const char* PropertyName)
{
	// 범위 클램프
	FogDensity = std::max(FogDensity, 0.0f);
	FogHeightFalloff = std::max(FogHeightFalloff, 0.0f);
	StartDistance = std::max(StartDistance, 0.0f);
	FogMaxOpacity = std::clamp(FogMaxOpacity, 0.0f, 1.0f);

	UPrimitiveComponent::PostEditProperty(PropertyName);
}

// -----------------------------------------------------------------------
//  Setter (런타임 호출용)
// -----------------------------------------------------------------------
void UHeightFogComponent::SetFogDensity(float V)
{
	FogDensity = std::max(V, 0.0f);
	NotifySpatialIndexDirty();
}

void UHeightFogComponent::SetFogHeightFalloff(float V)
{
	FogHeightFalloff = std::max(V, 0.0f);
	NotifySpatialIndexDirty();
}

void UHeightFogComponent::SetStartDistance(float V)
{
	StartDistance = std::max(V, 0.0f);
	NotifySpatialIndexDirty();
}

void UHeightFogComponent::SetFogCutoffDistance(float V)
{
	FogCutoffDistance = V;
	NotifySpatialIndexDirty();
}

void UHeightFogComponent::SetFogMaxOpacity(float V)
{
	FogMaxOpacity = std::clamp(V, 0.0f, 1.0f);
	NotifySpatialIndexDirty();
}

// -----------------------------------------------------------------------
//  CPU-side 안개 계산
//
//  Exponential Height Fog 공식 (UE4/5 동일 방식):
//
//    heightTerm  = FogHeightFalloff * max( WorldPos.Z - ComponentZ, 0 )
//    localDensity = FogDensity * exp( -heightTerm )
//
//    viewDist     = max( |WorldPos - CameraPos| - StartDistance, 0 )
//    [FogCutoffDistance > 0 이면 viewDist 를 CutoffDistance 로 클램프]
//
//    extinction   = 1 - exp( -localDensity * viewDist )
//    fogFactor    = clamp( extinction, 0, FogMaxOpacity )
// -----------------------------------------------------------------------
float UHeightFogComponent::ComputeFogFactor(const FVector& WorldPos, const FVector& CameraPos) const
{
	if (!bIsVisible || FogDensity <= MathUtil::Epsilon)
	{
		return 0.0f;
	}

	// 1. 높이 기반 로컬 밀도
	const float ComponentZ = GetWorldLocation().Z;
	const float HeightAbove = std::max(WorldPos.Z - ComponentZ, 0.0f);
	const float LocalDensity = FogDensity * std::exp(-FogHeightFalloff * HeightAbove);

	// 2. 뷰 거리 (StartDistance 이전은 0)
	float ViewDist = FVector::Distance(WorldPos, CameraPos) - StartDistance;
	ViewDist = std::max(ViewDist, 0.0f);

	if (FogCutoffDistance > 0.0f)
	{
		ViewDist = std::min(ViewDist, FogCutoffDistance);
	}

	// 3. Beer-Lambert 소광 → 안개 비율
	const float Extinction = 1.0f - std::exp(-LocalDensity * ViewDist);
	return std::clamp(Extinction, 0.0f, FogMaxOpacity);
}

FColor UHeightFogComponent::ApplyFog(const FColor& SceneColor,
	const FVector& WorldPos,
	const FVector& CameraPos) const
{
	const float FogFactor = ComputeFogFactor(WorldPos, CameraPos);

	// lerp(SceneColor, InscatteringColor, FogFactor)
	auto Lerp = [](uint8_t A, uint8_t B, float T) -> uint8_t
		{
			return static_cast<uint8_t>(std::clamp(A + T * (B - A), 0.0f, 255.0f));
		};

	return FColor(
		Lerp(SceneColor.R, FogInscatteringColor.R, FogFactor),
		Lerp(SceneColor.G, FogInscatteringColor.G, FogFactor),
		Lerp(SceneColor.B, FogInscatteringColor.B, FogFactor),
		SceneColor.A
	);
}