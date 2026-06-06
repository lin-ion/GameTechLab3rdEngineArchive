#include "GameFramework/Pawn/BossCharacter.h"

#include "Animation/AnimationManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/Montage/AnimMontage.h"
#include "Component/Primitive/SkeletalMeshComponent.h"

ABossCharacter::ABossCharacter()
{
	bAutoInputWASD = false;
	bAutoInputMouseLook = false;
}

void ABossCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	
	// Do Something?
}