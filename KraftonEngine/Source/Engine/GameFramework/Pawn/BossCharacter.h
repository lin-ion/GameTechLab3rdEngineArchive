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
	// 보스 체력이 0 이 된 프레임에 점수를 1회만 확정/기록하기 위한 런타임 가드.
	bool bScoreRecorded = false;
};
