#pragma once

#include "UI/Canvas/UITextElement.h"
#include "Object/Ptr/SoftObjectPtr.h"

#include "Source/Engine/UI/Canvas/UIImage.generated.h"

struct ID3D11Device;
struct ID3D11ShaderResourceView;
class UTexture2D;

// 이미지 요소(팔레트). [사이클 8] Texture 지정 시 텍스처 쿼드(텍스처 × BackgroundColor),
// 비면 단색(SimpleUIPass 가 1×1 흰색 fallback → BackgroundColor 그대로). 텍스트는 UUITextElement 에서 상속.
UCLASS()
class UUIImage : public UUITextElement
{
public:
	GENERATED_BODY()
	UUIImage()
	{
		SetSize(FVector2(200.0f, 200.0f));
		SetColor(FVector4(1.0f, 1.0f, 1.0f, 1.0f));   // 흰색 → 텍스처가 변조 없이 보임(단색 시 흰 배경)
	}

	// [사이클 8] TexturePath → SRV. 경로가 바뀔 때만 재로드(UTexture2D::LoadFromFile 캐시 활용).
	// 텍스처 없으면 nullptr → SimpleUIPass 가 흰색 fallback 으로 단색 처리. Device 는 패스가 전달.
	ID3D11ShaderResourceView* ResolveTextureSRV(ID3D11Device* Device);

private:
	UPROPERTY(Edit, Save, Category="Image", DisplayName="Texture", AssetType="Texture")
	FSoftObjectPtr TexturePath;

	// 런타임 캐시(UPROPERTY 아님 → 반사/직렬화 제외). 텍스처 수명은 UTexture2D 캐시가 GC-루팅.
	UTexture2D* LoadedTexture = nullptr;
	FString     ResolvedPath;
};
