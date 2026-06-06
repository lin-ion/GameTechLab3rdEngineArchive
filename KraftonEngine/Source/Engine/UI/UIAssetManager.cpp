#include "UI/UIAssetManager.h"

#include "Asset/AssetPackage.h"
#include "Object/GarbageCollection.h"
#include "Object/Object.h"
#include "Platform/Paths.h"
#include "UI/UIAsset.h"

UUIAsset* FUIAssetManager::Load(const FString& Path)
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);

	auto It = LoadedAssets.find(NormalizedPath);
	if (It != LoadedAssets.end())
	{
		if (IsValid(It->second))
		{
			return It->second;
		}
		LoadedAssets.erase(It);
	}

	if (!FAssetPackage::IsAssetPackagePath(NormalizedPath)) return nullptr;

	FAssetImportMetadata Metadata;
	FString              CanvasData;
	// 헤더 검증(EAssetPackageType::UI) + 메타데이터 + 트리 JSON 페이로드를 한 번에 읽는다.
	if (!FAssetPackage::LoadStringPayload(NormalizedPath, EAssetPackageType::UI, Metadata, CanvasData))
	{
		return nullptr;
	}

	UUIAsset* NewAsset = UObjectManager::Get().CreateObject<UUIAsset>();
	NewAsset->SetSourcePath(NormalizedPath);
	NewAsset->SetCanvasData(CanvasData);

	LoadedAssets.emplace(NormalizedPath, NewAsset);
	return NewAsset;
}

UUIAsset* FUIAssetManager::Find(const FString& Path) const
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);
	auto          It             = LoadedAssets.find(NormalizedPath);
	if (It == LoadedAssets.end())
	{
		return nullptr;
	}
	if (IsValid(It->second))
	{
		return It->second;
	}
	return nullptr;
}

bool FUIAssetManager::Save(UUIAsset* Asset)
{
	if (!Asset) return false;
	const FString& Path = Asset->GetSourcePath();
	if (Path.empty()) return false;

	const FString        NormalizedPath = FPaths::MakeProjectRelative(Path);
	FAssetImportMetadata Metadata;
	// 헤더(EAssetPackageType::UI) + 메타데이터 + 트리 JSON 페이로드를 순서대로 기록한다.
	if (!FAssetPackage::SaveStringPayload(NormalizedPath, EAssetPackageType::UI, Metadata, Asset->GetCanvasData()))
	{
		return false;
	}

	return true;
}

void FUIAssetManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& Pair : LoadedAssets)
	{
		Collector.AddReferencedObject(Pair.second);
	}
}

void FUIAssetManager::ClearCache()
{
	LoadedAssets.clear();
}
