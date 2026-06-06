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
};
