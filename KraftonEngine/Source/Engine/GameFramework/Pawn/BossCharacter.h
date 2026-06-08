#pragma once

#include "GameFramework/Pawn/Character.h"
#include "Source/Engine/GameFramework/Pawn/BossCharacter.generated.h"

UCLASS()
class ABossCharacter : public ACharacter
{
public:
	GENERATED_BODY()
	ABossCharacter();
	~ABossCharacter() override = default;

protected:
	void Tick(float DeltaTime) override;

private:
	// Score.uasset 결과 캔버스("ScoreBoard" 요소 보유)를 찾아 표시/숨김. 찾아서 적용했으면 true 반환.
	bool SetScoreUIVisible(bool bVisible);

	// 보스 체력이 0 이 된 프레임에 점수를 1회만 확정/기록하기 위한 런타임 가드.
	bool bScoreRecorded = false;
	// 결과 점수 UI 를 시작 시 1회 숨기기 위한 가드(캔버스 빌드 후 성공 시 set).
	bool bScoreUIInitialized = false;
};
