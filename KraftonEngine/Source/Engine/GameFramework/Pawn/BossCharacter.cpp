#include "GameFramework/Pawn/BossCharacter.h"

#include "Animation/AnimationManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/Montage/AnimMontage.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/ScoreManager.h"

ABossCharacter::ABossCharacter()
{
	bAutoInputWASD = false;
	bAutoInputMouseLook = false;
}

void ABossCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	
	// 보스 처치 감지(폴링) — 이 엔진엔 사망 이벤트가 없어 Haru 사망 애님과 동일하게 체력을 폴링한다.
	// 체력이 0 이 된 첫 프레임에 점수를 확정하고 Saves/Scores.json 에 기록한다(OnBossDefeated 가 1회 보장).
	if (!bScoreRecorded && GetCurrentHealth() <= 0.0f)
	{
		bScoreRecorded = true;
		FScoreManager::Get().OnBossDefeated();
	}
}