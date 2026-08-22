#include "DecalComponent.h"

#include <cmath>
#include <cstring>

#include "Core/ResourceManager.h"
#include "Engine/Geometry/OBB.h"
#include "Engine/GameFramework/AActor.h"

DEFINE_CLASS(UDecalComponent, UPrimitiveComponent)
REGISTER_FACTORY(UDecalComponent)

UDecalComponent* UDecalComponent::Duplicate()
{
    UDecalComponent* NewComp = UObjectManager::Get().CreateObject<UDecalComponent>();

    NewComp->SetActive(this->IsActive());
    NewComp->SetOwner(nullptr);

    NewComp->SetRelativeLocation(this->GetRelativeLocation());
    NewComp->SetRelativeRotation(this->GetRelativeRotation());
    NewComp->SetRelativeScale(this->GetRelativeScale());

    NewComp->SetVisibility(this->IsVisible());
	NewComp->bCreatedInEditorInstance = bCreatedInEditorInstance;

    NewComp->DecalTexturePath = this->DecalTexturePath;
    NewComp->CachedDecalTexture = this->GetCachedDecalTexture();
    NewComp->FadeAlpha = this->FadeAlpha;
    NewComp->FadeStartDelay = this->FadeStartDelay;
    NewComp->FadeInDuration = this->FadeInDuration;
    NewComp->FadeStartDelay = this->FadeStartDelay;
    NewComp->FadeDuration = this->FadeDuration;

    NewComp->DuplicateSubObjects();

    return NewComp;
}

void UDecalComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UPrimitiveComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Decal Texture Path", EPropertyType::String, &DecalTexturePath });
    OutProps.push_back({ "Fade In Start Delay", EPropertyType::Float, &FadeInStartDelay });
    OutProps.push_back({ "Fade In Duration", EPropertyType::Float, &FadeInDuration });
    OutProps.push_back({ "Fade Start Delay", EPropertyType::Float, &FadeStartDelay });
    OutProps.push_back({ "Fade Duration", EPropertyType::Float, &FadeDuration });
}

void UDecalComponent::PostEditProperty(const char* PropertyName)
{
    UPrimitiveComponent::PostEditProperty(PropertyName);

    if (strcmp(PropertyName, "Decal Texture Path") == 0)
    {
        SetDecalTexturePath(DecalTexturePath);
    }
    if (strcmp(PropertyName, "Fade In Start Delay") == 0)
    {
        if (FadeInStartDelay < 0.0f) { FadeInStartDelay = 0.0f; }
    }
    if (strcmp(PropertyName, "Fade In Duration") == 0)
    {
        if (FadeInDuration < 0.0f) { FadeInDuration = 0.0f; }
    }
    if (strcmp(PropertyName, "Fade Start Delay") == 0)
    {
        if (FadeStartDelay < 0.0f) { FadeStartDelay = 0.0f; }
    }
    if (strcmp(PropertyName, "Fade Duration") == 0)
    {
        if (FadeDuration < 0.0f) { FadeDuration = 0.0f; }
    }
}

void UDecalComponent::TickComponent(float DeltaTime)
{
    UpdateWorldAABB();

    FadeElapsedTime += DeltaTime;

    if (FadeInStartDelay == 0.0f && FadeInDuration == 0.0f)
    {
        return;
    }

    if (FadeElapsedTime < FadeInStartDelay)
    {
        FadeAlpha = 0.0f;
        return;
    }
    if (FadeElapsedTime < (FadeInStartDelay + FadeInDuration))
    {
        FadeAlpha = (FadeElapsedTime - FadeInStartDelay) / FadeInDuration;
        return;
    }

    // Persistent Decal
    if (FadeStartDelay == 0.0f && FadeDuration == 0.0f)
    {
        FadeAlpha = 1.0f;
        return;
    }

    if (FadeElapsedTime < (FadeInStartDelay + FadeInDuration + FadeStartDelay))
    {
        FadeAlpha = 1.0f;
        return;
    }
    if (FadeElapsedTime < (FadeInStartDelay + FadeInDuration + FadeStartDelay + FadeDuration))
    {
        FadeAlpha = 1.0f - ((FadeElapsedTime - (FadeInStartDelay + FadeInDuration + FadeStartDelay)) / FadeDuration);
        return;
    }

    FadeAlpha = 0.0f;

    if (AActor* Owner = GetOwner())
    {
        Owner->SetVisible(false);
        Owner->SetActive(false);
    }
}

FOBB UDecalComponent::GetDecalOBB() const
{
	const FVector WorldScale = GetWorldScale();
	const FVector WorldExtent(std::abs(WorldScale.X), std::abs(WorldScale.Y), std::abs(WorldScale.Z));
	FOBB DecalOBB;
	DecalOBB.Center = GetWorldLocation();
	DecalOBB.Extents = WorldExtent;
	DecalOBB.Axes[0] = GetForwardVector();
	DecalOBB.Axes[1] = GetRightVector();
	DecalOBB.Axes[2] = GetUpVector();
	return DecalOBB;
}

FMatrix UDecalComponent::GetDecalViewProjection() const
{
    // Scale이 half-extent라고 가정
    const FVector WorldScale = GetWorldScale();
    const FVector WorldExtent(std::abs(WorldScale.X), std::abs(WorldScale.Y), std::abs(WorldScale.Z));

    constexpr float NearPlane = 0.0f;
    float FarPlane = WorldExtent.X * 2.0f;
    float Width = WorldExtent.Y * 2.0f;
    float Height = WorldExtent.Z * 2.0f;
    if (FarPlane < 1e-4f) { FarPlane = 1e-4f; }
    if (Width < 1e-4f) { Width = 1e-4f; }
    if (Height < 1e-4f) { Height = 1e-4f; }
    const FMatrix Projection = FMatrix::MakeOrthographicLH(Width, Height, NearPlane, FarPlane);

    const FVector Eye = GetWorldLocation() - (GetForwardVector() * WorldExtent.X);
    const FVector Target = GetWorldLocation() + (GetForwardVector() * WorldExtent.X);
    const FMatrix View = FMatrix::MakeViewLookAtLH(Eye, Target, GetUpVector());
    return View * Projection;
}

void UDecalComponent::UpdateWorldAABB() const
{
    const FVector Forward = GetForwardVector();
    const FVector Right = GetRightVector();
    const FVector Up = GetUpVector();
    const FVector WorldScale = GetWorldScale();
    const FVector DecalExtent(std::abs(WorldScale.X), std::abs(WorldScale.Y), std::abs(WorldScale.Z));

    const FVector AABBExtent(
        std::abs(Forward.X) * DecalExtent.X + std::abs(Right.X) * DecalExtent.Y + std::abs(Up.X) * DecalExtent.Z,
        std::abs(Forward.Y) * DecalExtent.X + std::abs(Right.Y) * DecalExtent.Y + std::abs(Up.Y) * DecalExtent.Z,
        std::abs(Forward.Z) * DecalExtent.X + std::abs(Right.Z) * DecalExtent.Y + std::abs(Up.Z) * DecalExtent.Z);

    const FVector WorldCenter = GetWorldLocation();

    WorldAABB.Min = WorldCenter - AABBExtent;
    WorldAABB.Max = WorldCenter + AABBExtent;
}

bool UDecalComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
    return false;
}
