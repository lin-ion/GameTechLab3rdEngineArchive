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
	}
}

// Score.uasset 캔버스 식별: "ScoreBoard" 요소를 가진 AUICanvasActor. 찾으면 캔버스 전체 가시성을
// 토글(UUIElement::SetVisible — 숨김 시 서브트리 통째로 렌더 제외)하고 true 반환.
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
			Canvas->SetVisible(bVisible);
			return true;
		}
	}
	return false;
}