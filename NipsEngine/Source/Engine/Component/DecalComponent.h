#pragma once

#include "PrimitiveComponent.h"
#include "Core/ResourceManager.h"
#include "Core/ResourceTypes.h"

struct FTextureResource;
struct FOBB;

class UDecalComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UDecalComponent, UPrimitiveComponent)

	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Decal;
	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }

	UDecalComponent* Duplicate() override;
	UDecalComponent* DuplicateSubObjects() override { return this; }

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	void TickComponent(float DeltaTime) override;

	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;

	void SetDecalTexturePath(const FString& NewPath)
	{
		DecalTexturePath = NewPath;
		CachedDecalTexture = FResourceManager::Get().LoadTexture(NewPath);
	}

	const FString& GetDecalTexturePath() const { return DecalTexturePath; }

	FTextureResource* GetCachedDecalTexture()
	{
		if (CachedDecalTexture == nullptr)
		{
			CachedDecalTexture = FResourceManager::Get().LoadTexture(DecalTexturePath);
		}
		return CachedDecalTexture;
	}

	FOBB GetDecalOBB() const;

	FMatrix GetDecalViewProjection() const;

private:
	FString DecalTexturePath;
	FTextureResource* CachedDecalTexture = nullptr;
	float FadeElapsedTime = 0.0f;

public:
	float FadeAlpha = 1.0f;
	// Fade In을 시작하기 전까지 대기하는 시간
	float FadeInStartDelay = 0.0f;
	// Fade In이 완료되는 데 걸리는 시간
	float FadeInDuration = 0.0f;
	// Fade Out을 시작하기 전까지 대기하는 시간
	float FadeStartDelay = 0.0f;
	// Fade Out이 완료되는 데 걸리는 시간
	float FadeDuration = 0.0f;
	// Set fade duration and start delay to 0 to make persistent.
};
