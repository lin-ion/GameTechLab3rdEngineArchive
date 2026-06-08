#include "GameFramework/Pawn/BossCharacter.h"

#include "Animation/AnimationManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/Montage/AnimMontage.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/ScoreManager.h"
#include "GameFramework/World.h"
#include "UI/Canvas/UICanvasActor.h"
#include "UI/Canvas/UICanvas.h"
#include "UI/Canvas/UIElement.h"
#include "Runtime/Engine.h"
#include "Viewport/GameViewportClient.h"

ABossCharacter::ABossCharacter()
{
	bAutoInputWASD = false;
	bAutoInputMouseLook = false;
}

void ABossCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	
	// 결과 점수 UI 는 평소 숨김 — 첫 틱(모든 BeginPlay 이후, 캔버스 빌드 완료)에 찾아서 1회 숨긴다.
	// 아직 못 찾으면 다음 틱에 재시도.
	if (!bScoreUIInitialized)
	{
		if (SetScoreUIVisible(false))
		{
			bScoreUIInitialized = true;
		}
	}

	// 보스 처치 감지(폴링) — 이 엔진엔 사망 이벤트가 없어 Haru 사망 애님과 동일하게 체력을 폴링한다.
	// 체력이 0 이 된 첫 프레임에 점수를 확정/기록(OnBossDefeated 가 1회 보장)하고 결과 UI 를 표시한다.
	if (!bScoreRecorded && GetCurrentHealth() <= 0.0f)
	{
		bScoreRecorded = true;
		FScoreManager::Get().OnBossDefeated();
		SetScoreUIVisible(true);

		// 결과화면 입력 모드 — 마우스를 카메라(마우스룩)에서 분리하고 커서를 풀어 UI 클릭 가능하게.
		// UIOnly: 게임 입력 스냅샷을 만들지 않아 마우스룩/이동이 멈추고 커서 캡처도 풀리며,
		// UI 런타임 클릭(TickRuntimeInput)은 계속 처리된다(PIE/standalone 동일).
		if (GEngine)
		{
			if (UGameViewportClient* Viewport = GEngine->GetGameViewportClient())
			{
				Viewport->SetInputMode(EGameInputMode::UIOnly);
			}
		}
	}
}

namespace
{
	// 요소와 모든 하위 요소의 bVisible 를 일괄 설정. IsEffectivelyVisible 는 "자신 + 모든 조상"의
	// bVisible 를 보므로, 결과화면을 켜려면 루트뿐 아니라 하위 요소(ScoreBoard/ReturnTitle 등 —
	// 저작상 bVisible=false)의 bVisible 까지 직접 켜야 한다. (루트만 토글해 자식이 계속 숨겨지던 버그 수정.)
	void SetUISubtreeVisible(UUIElement* Element, bool bVisible)
	{
		if (!Element)
		{
			return;
		}
		Element->SetVisible(bVisible);
		for (USceneComponent* Child : Element->GetChildren())
		{
			if (UUIElement* UIChild = Cast<UUIElement>(Child))
			{
				SetUISubtreeVisible(UIChild, bVisible);
			}
		}
	}
}

// Score.uasset 캔버스 식별: "ScoreBoard" 요소를 가진 AUICanvasActor. 찾으면 캔버스 + 하위 요소
// 전체의 bVisible 를 토글하고 true 반환.
bool ABossCharacter::SetScoreUIVisible(bool bVisible)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	for (AActor* Actor : World->GetActors())
	{
		AUICanvasActor* CanvasActor = Cast<AUICanvasActor>(Actor);
		if (!CanvasActor)
		{
			continue;
		}
		UUICanvas* Canvas = CanvasActor->GetCanvas();
		if (!Canvas)
		{
			continue;
		}
		if (Canvas->FindByName(FString("ScoreBoard")))
		{
			SetUISubtreeVisible(Canvas, bVisible);
			return true;
		}
	}
	return false;
}