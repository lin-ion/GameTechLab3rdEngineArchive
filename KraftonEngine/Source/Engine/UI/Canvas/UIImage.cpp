#include "UI/Canvas/UIImage.h"

#include "Texture/Texture2D.h"

#include <d3d11.h>

ID3D11ShaderResourceView* UUIImage::ResolveTextureSRV(ID3D11Device* Device)
{
	// 경로가 바뀐 경우(또는 최초)에만 재로드. LoadFromFile 은 path#srgb 키로 캐시되어 매 프레임 재로드 없음.
	const FString& Path = TexturePath.ToString();
	if (Path != ResolvedPath)
	{
		ResolvedPath = Path;
		LoadedTexture = (TexturePath.IsNull() || !Device)
			? nullptr
			: UTexture2D::LoadFromFile(Path, Device, ETextureColorSpace::SRGB);
	}
	return (LoadedTexture && LoadedTexture->IsLoaded()) ? LoadedTexture->GetSRV() : nullptr;
}
