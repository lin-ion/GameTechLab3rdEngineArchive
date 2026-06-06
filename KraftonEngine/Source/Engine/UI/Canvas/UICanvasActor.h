#pragma once

#include "GameFramework/AActor.h"
#include "Object/Ptr/WeakObjectPtr.h"
#include "Core/Types/CoreTypes.h"

#include "Source/Engine/UI/Canvas/UICanvasActor.generated.h"

class UUICanvas;

// 신규 계층형 UI Canvas 를 소유하는 액터(진단 결정: Actor + Component → .Scene 직렬화).
// RootComponent 가 UUICanvas 이며, BeginPlay 에서 FUICanvasManager 에 등록한다.
// 액터가 월드에 살아있는 동안 AActor::AddReferencedObjects 가 RootComponent 를 통해
// 트리를 keepalive 하고, 매니저 등록으로 레이아웃/드로우 패스에 노출된다.
UCLASS()
class AUICanvasActor : public AActor
{
public:
	GENERATED_BODY()
	AUICanvasActor() { bNeedsTick = false; }

	void BeginPlay() override;
	void EndPlay() override;

	UUICanvas* GetCanvas() const { return Canvas.Get(); }

	// 루트 Canvas 구성(컴포넌트 생성 + RootComponent 설정). BeginPlay 또는 스폰 직후 호출.
	void InitCanvas();

	// UI .uasset(EAssetPackageType::UI) 경로에서 캔버스 트리를 복원해 RootComponent 로 세팅한다.
	// 트리 빌드 전용 — 화면 렌더 등록(FUICanvasManager::RegisterCanvas)은 여기서 하지 않고
	// BeginPlay 로 미룬다(진단 R1). 드롭/스폰 직후 또는 BeginPlay(에셋 경로 보유 시)에서 호출.
	void LoadFromAsset(const FString& InAssetPath);

private:
	TWeakObjectPtr<UUICanvas> Canvas = nullptr;
};
