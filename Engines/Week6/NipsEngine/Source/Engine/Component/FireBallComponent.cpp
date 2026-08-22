#include "FireBallComponent.h"
#include "GameFramework/World.h"
#include <cmath>

DEFINE_CLASS(UFireBallComponent, UPrimitiveComponent)
REGISTER_FACTORY(UFireBallComponent)

UFireBallComponent::UFireBallComponent()
{
    // 생성자 로직 (필요시 추가)
}

UPrimitiveComponent* UFireBallComponent::Duplicate()
{
	UFireBallComponent* NewComp = UObjectManager::Get().CreateObject<UFireBallComponent>();
	NewComp->SetActive(this->IsActive());
	NewComp->SetOwner(nullptr);

	NewComp->SetRelativeLocation(this->GetRelativeLocation());
	NewComp->SetRelativeRotation(this->GetRelativeRotation());
	NewComp->SetRelativeScale(this->GetRelativeScale());

	NewComp->SetVisibility(this->IsVisible());

	NewComp->Intensity = Intensity;
	NewComp->Radius = Radius;
	NewComp->RadiusFallOff = RadiusFallOff;
	NewComp->Color = Color;

    // 자신의 복사본을 생성하여 반환합니다.
	return NewComp;
}

void UFireBallComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    // 부모 클래스의 프로퍼티를 먼저 담습니다.
    UPrimitiveComponent::GetEditableProperties(OutProps);

    // 제공해주신 FPropertyDescriptor 구조체 포맷에 맞춰 FireBall의 멤버 변수들을 노출합니다.
    // { "표시이름", 타입, 포인터, Min, Max, Speed(드래그 민감도) }
    OutProps.push_back({"Intensity", EPropertyType::Float, &Intensity, 0.0f, 100.0f, 0.1f});
    OutProps.push_back({"Radius", EPropertyType::Float, &Radius, 0.0f, 1000.0f, 1.0f});
    OutProps.push_back({"RadiusFallOff", EPropertyType::Float, &RadiusFallOff, 0.0f, 1000.0f, 1.0f});
    OutProps.push_back({"Color", EPropertyType::Vec4, &Color});
}

void UFireBallComponent::UpdateWorldAABB() const
{
    // Fireball의 중심점
    FVector Center = GetWorldLocation();

    // AABB는 시각적 처리 및 Broadphase 충돌을 커버해야 합니다.
    // 따라서 순수 충돌 반경(Radius)에 특수효과 반경(RadiusFallOff)을 더한 최대 범위를 계산합니다.
	float MaxExtent = RadiusFallOff;
    FVector ExtentVector(MaxExtent, MaxExtent, MaxExtent);

    // FAABB의 정의에 따라 Min/Max 또는 Center/Extents를 세팅합니다. (일반적인 Min/Max 구조 기준)
    WorldAABB.Min = Center - ExtentVector;
    WorldAABB.Max = Center + ExtentVector;
}

bool UFireBallComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
    FVector Center = GetWorldLocation();
    FVector L = Center - Ray.Origin;

    // tca = L을 Ray 방향으로 투영한 길이 (Ray.Direction은 정규화되어 있다고 가정)
    float tca = FVector::DotProduct(L, Ray.Direction);

    // tca가 0보다 작으면 구(FireBall)가 Ray의 반대 방향(뒤)에 있는 것입니다.
    if (tca < 0.0f)
    {
        return false;
    }

    // 구의 중심에서 Ray까지의 최단 거리의 제곱 (피타고라스 정리)
    float d2 = FVector::DotProduct(L, L) - (tca * tca);

    // 요구사항: RayCast는 Fireball의 Primitive(Radius)까지만 영향을 받음
    float Radius2 = Radius * Radius;
    if (d2 > Radius2)
    {
        // Ray가 Radius 범위를 완전히 빗나감 (FallOff 영역은 무시)
        return false;
    }

    // 교차점까지의 거리 계산
    float thc = std::sqrt(Radius2 - d2);
    float t0 = tca - thc; // Ray가 구에 진입하는 첫 번째 타격점

    // 타격점이 Ray 시작점보다 뒤에 있을 수 있으므로 예외 처리 (Ray 원점이 구 내부에 있을 경우)
    if (t0 < 0.0f)
    {
        t0 = tca + thc; // 구 내부에서 밖으로 나가는 점을 타격점으로 간주
        if (t0 < 0.0f)
            return false;
    }

    // HitResult 갱신
    OutHitResult.bHit = true;
    OutHitResult.Distance = t0;
    OutHitResult.HitComponent = this;

    // 만약 OutHitResult 구조체에 HitPoint(타격 위치)나 Normal(법선 벡터)이 있다면 아래처럼 계산해 넣으면 됩니다.
    // OutHitResult.ImpactPoint = Ray.Origin + (Ray.Direction * t0);
    // OutHitResult.Normal = (OutHitResult.ImpactPoint - Center).GetSafeNormal();

    return true;
}

EPrimitiveType UFireBallComponent::GetPrimitiveType() const
{
    // FireBall은 기본적으로 구체 형태이므로 EPT_Sphere를 반환하거나,
    // EPrimitiveType 열거형에 EPT_FireBall 등이 추가되어 있다면 그것을 반환하면 됩니다.
    return EPrimitiveType::EPT_FireBall;
}