#pragma once
#include "PrimitiveComponent.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"


class FViewportCamera;
struct FTextureResource;

class UBillboardComponent : public UPrimitiveComponent
{
protected:
	bool bIsBillboard = true;
	bool bIsCylindrical = false; // true면 카메라의 Up 벡터는 무시하고 월드 Up 벡터를 사용합니다.
	bool TryGetActiveCamera(const FViewportCamera*& OutCamera) const;
	
	// UpdateWorldAABB 등의 함수를 오버라이드하지 않았기 때문에 UBillboradComponent도 추상 클래스가 됩니다.
	// 추후에 UBillboardComponent를 사용할 일이 있다면 Duplicate의 주석을 해제하고 수정하시면 됩니다.
	virtual UBillboardComponent* Duplicate() override;
	virtual UBillboardComponent* DuplicateSubObjects() override  { return this; }

public:
	DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)
	
	void TickComponent(float DeltaTime) override;

	void SetBillboardEnabled(bool bEnable) { bIsBillboard = bEnable; }
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Billboard;

	static FMatrix MakeBillboardWorldMatrix(
		const FVector& WorldLocation,
		const FVector& WorldScale,
		const FVector& CameraForward,
		const FVector& CameraRight,
		const FVector& CameraUp);

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
	bool SupportsOutline() const override { return false; }

	void SetTextureName(FString InName);
	FString GetTexturePath();
	FMaterialResource* GetCachedSprite();
	void PostEditProperty(const char* PropertyName) override;

	//////////////////// override ////////////////////////////
	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void SetSpriteSize(float InWidth, float InHeight) { Width = InWidth; Height = InHeight; }
	float GetWidth()  const { return Width; }
	float GetHeight() const { return Height; }

	///////////////////////////////////////////////////////////

private:
	FString BillboardTexturePath;
	FMaterialResource* CachedSprite = { nullptr }; // ResourceManager 소유, 여기선 참조만
	float  Width = 1.0f;
	float  Height = 1.0f;
};

